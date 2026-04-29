#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <tuple>
#include <utility>

#include "vocals.hpp"

namespace {
constexpr double SP_BUCKET_SCALE = 1000.0;
constexpr double SP_EPSILON = 0.000001;
constexpr double FULL_SP_UNITS = 8.0;
constexpr double MIN_INTERNAL_WINDOW_SECONDS = 0.5;
constexpr std::int64_t SCORE_UNIT_SCALE = 1000000;

using BeatRange = std::tuple<SightRead::Beat, SightRead::Beat>;
using ScoreUnits = std::int64_t;

double clamp_sp_units(double value)
{
    return std::clamp(value, 0.0, FULL_SP_UNITS);
}

double phrase_sp_units(const SpEngineValues& sp_engine_values)
{
    return FULL_SP_UNITS * sp_engine_values.phrase_amount;
}

double activation_threshold_units(const SpEngineValues& sp_engine_values)
{
    return FULL_SP_UNITS * sp_engine_values.minimum_to_activate;
}

int to_bucket(double sp_units)
{
    return static_cast<int>(std::lround(clamp_sp_units(sp_units)
                                        * SP_BUCKET_SCALE));
}

double from_bucket(int bucket)
{
    return clamp_sp_units(bucket / SP_BUCKET_SCALE);
}

ScoreUnits to_score_units(double score)
{
    return static_cast<ScoreUnits>(std::llround(score * SCORE_UNIT_SCALE));
}

int score_points(ScoreUnits score_units)
{
    return static_cast<int>(
        std::llround(static_cast<double>(score_units) / SCORE_UNIT_SCALE));
}

std::size_t measure_index_for(const std::vector<double>& measure_lines,
                              double beat)
{
    auto measure_iter = std::upper_bound(measure_lines.cbegin() + 1,
                                         measure_lines.cend(), beat);
    if (measure_iter == measure_lines.cbegin() + 1) {
        return 0;
    }
    return static_cast<std::size_t>(
        std::distance(measure_lines.cbegin(), measure_iter) - 2);
}

bool has_width(SightRead::Beat start, SightRead::Beat end)
{
    return end.value() - start.value() > SP_EPSILON;
}

double range_duration_seconds(const SightRead::TempoMap& tempo_map,
                              SightRead::Beat start, SightRead::Beat end)
{
    if (!has_width(start, end)) {
        return 0.0;
    }
    return (tempo_map.to_seconds(end) - tempo_map.to_seconds(start)).value();
}

bool is_large_internal_window(const SightRead::TempoMap& tempo_map,
                              SightRead::Beat start, SightRead::Beat end)
{
    return has_width(start, end)
        && range_duration_seconds(tempo_map, start, end)
            >= MIN_INTERNAL_WINDOW_SECONDS;
}

SightRead::Beat latest_start_in_window(SightRead::Beat start, SightRead::Beat end)
{
    return SightRead::Beat {std::nextafter(end.value(), start.value())};
}

std::vector<BeatRange> merged_phrase_segments(
    const SightRead::VocalPhrase& phrase,
    const std::vector<SightRead::VocalTube>& tubes, const SpTimeMap& time_map)
{
    const auto phrase_end = phrase.position + phrase.length;
    std::vector<BeatRange> segments;
    for (const auto& tube : tubes) {
        const auto tube_end = tube.position + tube.length;
        if (tube.position >= phrase_end) {
            break;
        }
        if (tube_end <= phrase.position) {
            continue;
        }

        const auto clipped_start = std::max(tube.position, phrase.position);
        const auto clipped_end = std::min(tube_end, phrase_end);
        const auto start = time_map.to_beats(clipped_start);
        const auto end = time_map.to_beats(clipped_end);
        if (has_width(start, end)) {
            segments.emplace_back(start, end);
        }
    }

    std::ranges::sort(segments, [](const auto& lhs, const auto& rhs) {
        return std::get<0>(lhs).value() < std::get<0>(rhs).value();
    });

    std::vector<BeatRange> merged_segments;
    for (const auto& segment : segments) {
        if (merged_segments.empty()
            || std::get<0>(segment).value()
                > std::get<1>(merged_segments.back()).value() + SP_EPSILON) {
            merged_segments.push_back(segment);
            continue;
        }

        if (std::get<1>(segment).value()
            > std::get<1>(merged_segments.back()).value()) {
            std::get<1>(merged_segments.back()) = std::get<1>(segment);
        }
    }
    return merged_segments;
}

int vocal_phrase_base_score(bool rb_scoring, int multiplier,
                            int pitched_tube_count)
{
    if (rb_scoring) {
        return 1000 * multiplier;
    }
    return 1000 * multiplier + 100 * pitched_tube_count;
}

void add_measure_boundaries(const SightRead::TempoMap& tempo_map,
                            SightRead::Beat start, SightRead::Beat end,
                            std::vector<SightRead::Beat>& boundaries)
{
    const auto start_measure = tempo_map.to_measures(start).value();
    const auto end_measure = tempo_map.to_measures(end).value();
    const auto first_measure = static_cast<int>(std::floor(start_measure)) + 1;
    const auto last_measure = static_cast<int>(std::ceil(end_measure));
    for (auto measure = first_measure; measure < last_measure; ++measure) {
        const auto boundary = tempo_map.to_beats(
            SightRead::Measure {static_cast<double>(measure)});
        if (boundary.value() > start.value() + SP_EPSILON
            && boundary.value() < end.value() - SP_EPSILON) {
            boundaries.push_back(boundary);
        }
    }
}

double tube_units(const VocalPieProfile& profile, double duration_beats,
                  double duration_seconds, SightRead::VocalTubeType tube_type)
{
    auto units = duration_beats * profile.duration_weight_per_beat
        + profile.tube_weight_beats;
    if (profile.short_tube_threshold_ms > 0.0
        && duration_seconds * 1000.0 <= profile.short_tube_threshold_ms) {
        units *= profile.short_tube_multiplier;
    }
    if (tube_type == SightRead::VocalTubeType::Unpitched) {
        units *= profile.nonpitch_multiplier;
    }
    return units;
}

VocalPhrasePie build_phrase_pie(const SightRead::VocalPhrase& phrase,
                                const std::vector<SightRead::VocalTube>& tubes,
                                const SightRead::TempoMap& tempo_map,
                                const SpTimeMap& time_map,
                                const VocalPieProfile& profile)
{
    VocalPhrasePie pie;
    const auto phrase_end = phrase.position + phrase.length;
    for (const auto& tube : tubes) {
        const auto tube_end = tube.position + tube.length;
        if (tube.position >= phrase_end) {
            break;
        }
        if (tube_end <= phrase.position) {
            continue;
        }

        const auto clipped_start = std::max(tube.position, phrase.position);
        const auto clipped_end = std::min(tube_end, phrase_end);
        const auto start = time_map.to_beats(clipped_start);
        const auto end = time_map.to_beats(clipped_end);
        if (!has_width(start, end)) {
            continue;
        }

        const auto duration_beats = end.value() - start.value();
        const auto duration_seconds
            = range_duration_seconds(tempo_map, start, end);
        const auto normal_units
            = tube_units(profile, duration_beats, duration_seconds, tube.type);
        const auto perfect_units = normal_units
            * (tube.type == SightRead::VocalTubeType::Pitched
                   ? profile.perfect_vibrato_multiplier
                   : 1.0);
        pie.target_units += normal_units;

        std::vector<SightRead::Beat> boundaries {start, end};
        add_measure_boundaries(tempo_map, start, end, boundaries);
        std::ranges::sort(boundaries, [](const auto& lhs, const auto& rhs) {
            return lhs.value() < rhs.value();
        });

        for (std::size_t i = 1; i < boundaries.size(); ++i) {
            const auto segment_start = boundaries.at(i - 1);
            const auto segment_end = boundaries.at(i);
            if (!has_width(segment_start, segment_end)) {
                continue;
            }
            const auto proportion
                = (segment_end.value() - segment_start.value()) / duration_beats;
            pie.segments.push_back({.start = segment_start,
                                    .end = segment_end,
                                    .normal_units = normal_units * proportion,
                                    .perfect_units
                                    = perfect_units * proportion});
        }
    }

    std::ranges::sort(pie.segments, [](const auto& lhs, const auto& rhs) {
        if (lhs.start.value() != rhs.start.value()) {
            return lhs.start.value() < rhs.start.value();
        }
        return lhs.end.value() < rhs.end.value();
    });
    return pie;
}

void add_activation_start(std::vector<SightRead::Beat>& starts,
                          SightRead::Beat start)
{
    const auto already_present
        = std::ranges::any_of(starts, [start](const auto& existing) {
              return std::abs(existing.value() - start.value()) < SP_EPSILON;
          });
    if (!already_present) {
        starts.push_back(start);
    }
}

std::optional<SightRead::Beat>
latest_zero_prefill_start(const VocalPhraseInfo& phrase, SightRead::Beat start,
                          SightRead::Beat end)
{
    if (phrase.pie.target_units <= SP_EPSILON) {
        return std::nullopt;
    }

    const auto latest = latest_start_in_window(start, end);
    if (phrase.pie.required_prefill(latest) <= SP_EPSILON) {
        return latest;
    }
    if (phrase.pie.required_prefill(start) > SP_EPSILON) {
        return std::nullopt;
    }

    auto low = start.value();
    auto high = latest.value();
    for (auto i = 0; i < 60; ++i) {
        const auto mid = (low + high) / 2.0;
        if (phrase.pie.required_prefill(SightRead::Beat {mid})
            <= SP_EPSILON) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return SightRead::Beat {low};
}

std::string esf_annotation(const VocalPhraseInfo& phrase,
                           SightRead::Beat boosted_start,
                           double required_prefill_fraction,
                           double boosted_fraction)
{
    if (boosted_fraction <= SP_EPSILON
        || boosted_start.value() <= phrase.start.value() + SP_EPSILON
        || boosted_start.value() >= phrase.end.value() - SP_EPSILON) {
        return "";
    }
    if (required_prefill_fraction <= SP_EPSILON) {
        return "S";
    }
    return "S" + std::to_string(static_cast<int>(std::ceil(
                     required_prefill_fraction * 100.0 - SP_EPSILON)));
}

struct PhraseScoreGain {
    ScoreUnits score_units;
    double boosted_fraction;
    double required_prefill_fraction;
    std::string esf_annotation;
};

PhraseScoreGain phrase_score_gain(const VocalPhraseInfo& phrase,
                                  SightRead::Beat boosted_start,
                                  SightRead::Beat boosted_end)
{
    if (!has_width(boosted_start, boosted_end)
        || phrase.pie.target_units <= SP_EPSILON) {
        return {};
    }

    const auto required_prefill = phrase.pie.required_prefill(boosted_start);
    const auto boosted_fraction
        = phrase.pie.boosted_fraction(boosted_start, boosted_end);
    return {.score_units = to_score_units(phrase.base_score * boosted_fraction),
            .boosted_fraction = boosted_fraction,
            .required_prefill_fraction = required_prefill,
            .esf_annotation = esf_annotation(
                phrase, boosted_start, required_prefill, boosted_fraction)};
}

struct DpKey {
    std::size_t phrase_index;
    bool active;
    int sp_bucket;

    auto operator<=>(const DpKey&) const = default;
};

struct DpValue {
    ScoreUnits score;
    std::optional<std::size_t> activation_choice;
};

struct AdvanceResult {
    DpKey next_key;
    ScoreUnits score_gain;
    double boosted_pie_fraction;
    double required_prefill_fraction;
    std::string esf_annotation;
    bool phrase_boosted;
    bool ended_activation;
    SightRead::Beat activation_end;
};

AdvanceResult advance_phrase(const VocalsProcessedSong& song, const DpKey& key,
                             const std::optional<SightRead::Beat>& activation_start)
{
    const auto& phrase = song.phrases().at(key.phrase_index);
    double sp_units = from_bucket(key.sp_bucket);
    auto active = key.active;
    auto ended_activation = false;
    auto activation_end = phrase.end;
    auto phrase_active_start_sp = phrase.start_sp;
    std::optional<SightRead::Beat> boosted_start;
    auto score_gain = ScoreUnits {0};
    auto boosted_pie_fraction = 0.0;
    auto required_prefill_fraction = 0.0;
    std::string esf_text;

    if (!active && activation_start.has_value()) {
        active = true;
        boosted_start = *activation_start;
        phrase_active_start_sp = song.time_map().to_sp_measures(*activation_start);
    } else if (active) {
        boosted_start = phrase.start;
    }

    const auto sp_phrase_units = phrase_sp_units(song.sp_engine_values());

    if (active) {
        const auto phrase_duration
            = (phrase.end_sp - phrase_active_start_sp).value();
        if (sp_units + SP_EPSILON >= phrase_duration) {
            const auto gain
                = phrase_score_gain(phrase, *boosted_start, phrase.end);
            score_gain = gain.score_units;
            boosted_pie_fraction = gain.boosted_fraction;
            required_prefill_fraction = gain.required_prefill_fraction;
            esf_text = gain.esf_annotation;
            sp_units = clamp_sp_units(sp_units - phrase_duration);
            if (phrase.is_sp_phrase) {
                sp_units = clamp_sp_units(sp_units + sp_phrase_units);
            }
        } else {
            activation_end = song.time_map().to_beats(phrase_active_start_sp
                                                      + SpMeasure {sp_units});
            const auto gain
                = phrase_score_gain(phrase, *boosted_start, activation_end);
            score_gain = gain.score_units;
            boosted_pie_fraction = gain.boosted_fraction;
            required_prefill_fraction = gain.required_prefill_fraction;
            esf_text = gain.esf_annotation;
            ended_activation = true;
            active = false;
            sp_units = phrase.is_sp_phrase ? sp_phrase_units : 0.0;
        }
    } else if (phrase.is_sp_phrase) {
        sp_units = clamp_sp_units(sp_units + sp_phrase_units);
    }

    if (active && key.phrase_index + 1 < song.phrases().size()) {
        const auto& next_phrase = song.phrases().at(key.phrase_index + 1);
        const auto gap_duration
            = (next_phrase.start_sp - phrase.end_sp).value();
        if (sp_units + SP_EPSILON >= gap_duration) {
            sp_units = clamp_sp_units(sp_units - gap_duration);
        } else {
            ended_activation = true;
            activation_end
                = song.time_map().to_beats(phrase.end_sp + SpMeasure {sp_units});
            active = false;
            sp_units = 0.0;
        }
    }

    return {.next_key = {.phrase_index = key.phrase_index + 1,
                         .active = active,
                         .sp_bucket = to_bucket(sp_units)},
            .score_gain = score_gain,
            .boosted_pie_fraction = boosted_pie_fraction,
            .required_prefill_fraction = required_prefill_fraction,
            .esf_annotation = std::move(esf_text),
            .phrase_boosted = score_gain > 0,
            .ended_activation = ended_activation,
            .activation_end = activation_end};
}

std::map<DpKey, DpValue> solve_best_paths(const VocalsProcessedSong& song)
{
    std::map<DpKey, DpValue> memo;

    const auto solve = [&](const auto& self, const DpKey& key) -> ScoreUnits {
        if (key.phrase_index >= song.phrases().size()) {
            memo.emplace(
                key, DpValue {.score = 0, .activation_choice = std::nullopt});
            return 0;
        }
        if (const auto iter = memo.find(key); iter != memo.cend()) {
            return iter->second.score;
        }

        const auto no_act_result = advance_phrase(song, key, std::nullopt);
        auto best_score
            = no_act_result.score_gain + self(self, no_act_result.next_key);
        std::optional<std::size_t> best_activation_choice;

        const auto can_activate = !key.active
            && from_bucket(key.sp_bucket) + SP_EPSILON
                >= activation_threshold_units(song.sp_engine_values());
        if (can_activate) {
            const auto& activation_starts
                = song.activation_starts(key.phrase_index);
            auto best_activation_score = std::numeric_limits<ScoreUnits>::min();
            for (std::size_t i = 0; i < activation_starts.size(); ++i) {
                const auto act_result
                    = advance_phrase(song, key, activation_starts.at(i));
                const auto act_score
                    = act_result.score_gain + self(self, act_result.next_key);
                if (act_score > best_activation_score
                    || (act_score == best_activation_score
                        && (!best_activation_choice.has_value()
                            || activation_starts.at(i).value()
                                > activation_starts
                                      .at(*best_activation_choice)
                                      .value()))) {
                    best_activation_score = act_score;
                    best_activation_choice = i;
                }
            }

            if (best_activation_choice.has_value()
                && best_activation_score > best_score) {
                best_score = best_activation_score;
            } else {
                best_activation_choice = std::nullopt;
            }
        }

        memo.emplace(key, DpValue {.score = best_score,
                                   .activation_choice = best_activation_choice});
        return best_score;
    };

    solve(solve, {.phrase_index = 0, .active = false, .sp_bucket = 0});
    return memo;
}
}

VocalPieProfile VocalPieProfile::default_profile()
{
    return {.duration_weight_per_beat = 1.0,
            .tube_weight_beats = 0.25,
            .perfect_vibrato_multiplier = 1.67,
            .nonpitch_multiplier = 1.0,
            .short_tube_threshold_ms = 0.0,
            .short_tube_multiplier = 1.0};
}

VocalPieProfile VocalPieProfile::rb3_default()
{
    return default_profile();
}

double VocalPhrasePie::raw_units_between(SightRead::Beat start,
                                         SightRead::Beat end,
                                         bool perfect_vibrato) const
{
    if (!has_width(start, end)) {
        return 0.0;
    }

    auto units = 0.0;
    for (const auto& segment : segments) {
        const auto clipped_start = std::max(segment.start, start);
        const auto clipped_end = std::min(segment.end, end);
        if (!has_width(clipped_start, clipped_end)) {
            continue;
        }
        const auto segment_width = segment.end.value() - segment.start.value();
        const auto clipped_width = clipped_end.value() - clipped_start.value();
        units += (perfect_vibrato ? segment.perfect_units : segment.normal_units)
            * clipped_width / segment_width;
    }
    return units;
}

double VocalPhrasePie::fill_between(SightRead::Beat start, SightRead::Beat end,
                                    bool perfect_vibrato) const
{
    if (target_units <= SP_EPSILON) {
        return 0.0;
    }
    return std::clamp(raw_units_between(start, end, perfect_vibrato)
                          / target_units,
                      0.0, 1.0);
}

double VocalPhrasePie::suffix_capacity(SightRead::Beat start) const
{
    if (segments.empty()) {
        return 0.0;
    }
    return fill_between(start, segments.back().end, true);
}

double VocalPhrasePie::required_prefill(SightRead::Beat start) const
{
    return std::max(0.0, 1.0 - suffix_capacity(start));
}

double VocalPhrasePie::boosted_fraction(SightRead::Beat active_start,
                                        SightRead::Beat active_end) const
{
    const auto required = required_prefill(active_start);
    return std::min(fill_between(active_start, active_end, true),
                    1.0 - required);
}

VocalsProcessedSong::VocalsProcessedSong(const SightRead::VocalTrack& track,
                                         const PathingSettings& pathing_settings)
    : m_tempo_map {track.global_data().tempo_map()}
    , m_time_map {m_tempo_map, pathing_settings.engine->sp_mode()}
    , m_sp_engine_values {pathing_settings.engine->sp_engine_values()}
{
    const auto& tubes = track.tubes();
    const auto rb_scoring = pathing_settings.engine->is_rock_band();
    const auto max_multiplier = pathing_settings.engine->max_multiplier();
    const auto pie_profile = rb_scoring ? VocalPieProfile::rb3_default()
                                        : VocalPieProfile::default_profile();
    m_phrases.reserve(track.phrases().size());
    m_activation_starts.resize(track.phrases().size());

    const auto add_window = [this](std::size_t target_phrase_index,
                                   SightRead::Beat start, SightRead::Beat end) {
        if (!has_width(start, end)) {
            return;
        }

        m_activation_windows.push_back({.target_phrase_index = target_phrase_index,
                                        .start = start,
                                        .end = end});
        auto& starts = m_activation_starts.at(target_phrase_index);
        add_activation_start(starts, start);
        add_activation_start(starts, latest_start_in_window(start, end));
        if (target_phrase_index < m_phrases.size()) {
            const auto zero_prefill_start = latest_zero_prefill_start(
                m_phrases.at(target_phrase_index), start, end);
            if (zero_prefill_start.has_value()) {
                add_activation_start(starts, *zero_prefill_start);
            }
        }
    };
    const auto add_internal_window = [&](std::size_t target_phrase_index,
                                         SightRead::Beat start,
                                         SightRead::Beat end) {
        if (is_large_internal_window(m_tempo_map, start, end)) {
            add_window(target_phrase_index, start, end);
        }
    };

    for (std::size_t i = 0; i < track.phrases().size(); ++i) {
        const auto& phrase = track.phrases().at(i);
        const auto phrase_start = m_time_map.to_beats(phrase.position);
        const auto phrase_end
            = m_time_map.to_beats(phrase.position + phrase.length);
        const auto merged_segments = merged_phrase_segments(phrase, tubes, m_time_map);
        auto pie = build_phrase_pie(phrase, tubes, m_tempo_map, m_time_map,
                                    pie_profile);
        std::vector<VocalScoredSegment> scored_segments;
        scored_segments.reserve(merged_segments.size());
        auto total_scored_seconds = 0.0;
        auto pitched_tube_count = 0;
        for (const auto& [start, end] : merged_segments) {
            const auto duration_seconds
                = range_duration_seconds(m_tempo_map, start, end);
            scored_segments.push_back(
                {.start = start, .end = end, .duration_seconds = duration_seconds});
            total_scored_seconds += duration_seconds;
        }
        for (const auto& tube : tubes) {
            const auto tube_end = tube.position + tube.length;
            if (tube.position >= phrase.position + phrase.length) {
                break;
            }
            if (tube_end <= phrase.position) {
                continue;
            }
            if (tube.type == SightRead::VocalTubeType::Pitched
                && tube.position >= phrase.position
                && tube_end <= phrase.position + phrase.length) {
                ++pitched_tube_count;
            }
        }

        const auto multiplier
            = std::min(max_multiplier, static_cast<int>(i) + 1);
        const auto base_score
            = vocal_phrase_base_score(rb_scoring, multiplier, pitched_tube_count);
        m_total_base_score += base_score;
        m_phrases.push_back({.start = phrase_start,
                             .end = phrase_end,
                             .start_sp = m_time_map.to_sp_measures(phrase_start),
                             .end_sp = m_time_map.to_sp_measures(phrase_end),
                             .is_sp_phrase = phrase.is_sp_phrase,
                             .multiplier = multiplier,
                             .base_score = base_score,
                             .total_scored_seconds = total_scored_seconds,
                             .scored_segments = std::move(scored_segments),
                             .pie = std::move(pie)});

        if (merged_segments.empty()) {
            add_internal_window(i, phrase_start, phrase_end);
            continue;
        }

        add_internal_window(i, phrase_start, std::get<0>(merged_segments.front()));
        for (std::size_t segment_index = 1; segment_index < merged_segments.size();
             ++segment_index) {
            const auto gap_start
                = std::get<1>(merged_segments.at(segment_index - 1));
            const auto gap_end = std::get<0>(merged_segments.at(segment_index));
            add_internal_window(i, gap_start, gap_end);
        }
        add_internal_window(i, std::get<1>(merged_segments.back()), phrase_end);
    }

    for (std::size_t i = 1; i < m_phrases.size(); ++i) {
        const auto& previous = m_phrases.at(i - 1);
        const auto& current = m_phrases.at(i);
        add_internal_window(i, previous.end, current.start);
    }

    std::ranges::sort(m_activation_windows, [](const auto& lhs, const auto& rhs) {
        if (lhs.start.value() != rhs.start.value()) {
            return lhs.start.value() < rhs.start.value();
        }
        if (lhs.end.value() != rhs.end.value()) {
            return lhs.end.value() < rhs.end.value();
        }
        return lhs.target_phrase_index < rhs.target_phrase_index;
    });
    for (auto& starts : m_activation_starts) {
        std::ranges::sort(starts, [](const auto& lhs, const auto& rhs) {
            return lhs.value() < rhs.value();
        });
        starts.erase(std::ranges::unique(starts, [](const auto& lhs,
                                                    const auto& rhs) {
                         return std::abs(lhs.value() - rhs.value())
                             < SP_EPSILON;
                     }).begin(),
                     starts.end());
    }
}

std::vector<int> VocalsProcessedSong::base_score_values(
    const std::vector<double>& measure_lines) const
{
    std::vector<int> values(measure_lines.size() - 1, 0);
    for (const auto& phrase : m_phrases) {
        values.at(measure_index_for(measure_lines, phrase.end.value()))
            += phrase.base_score;
    }
    return values;
}

std::vector<int> VocalsProcessedSong::running_score_values(
    const std::vector<double>& measure_lines, const VocalPath& path) const
{
    auto values = base_score_values(measure_lines);
    const auto phrase_count
        = std::min(m_phrases.size(), path.phrase_score_boosts.size());
    for (std::size_t phrase_index = 0; phrase_index < phrase_count;
         ++phrase_index) {
        values.at(measure_index_for(measure_lines,
                                    m_phrases.at(phrase_index).end.value()))
            += path.phrase_score_boosts.at(phrase_index);
    }

    int running_total = 0;
    for (auto& value : values) {
        running_total += value;
        value = running_total;
    }
    return values;
}

std::vector<double> VocalsProcessedSong::sp_percent_values(
    const std::vector<double>& measure_lines, const VocalPath& path) const
{
    enum class EventType : std::uint8_t { ActStart, ActEnd, SpPhrase, Measure };

    std::vector<std::tuple<SpMeasure, EventType>> events;
    events.reserve(measure_lines.size() + m_phrases.size()
                   + path.activations.size() * 2);
    for (auto it = std::next(measure_lines.cbegin()); it < measure_lines.cend();
         ++it) {
        events.emplace_back(m_time_map.to_sp_measures(SightRead::Beat {*it}),
                            EventType::Measure);
    }
    for (const auto& phrase : m_phrases) {
        if (phrase.is_sp_phrase) {
            events.emplace_back(phrase.end_sp, EventType::SpPhrase);
        }
    }
    for (const auto& act : path.activations) {
        events.emplace_back(m_time_map.to_sp_measures(act.start),
                            EventType::ActStart);
        events.emplace_back(m_time_map.to_sp_measures(act.end),
                            EventType::ActEnd);
    }

    std::ranges::sort(events, [](const auto& lhs, const auto& rhs) {
        const auto lhs_value = std::get<0>(lhs).value();
        const auto rhs_value = std::get<0>(rhs).value();
        if (lhs_value != rhs_value) {
            return lhs_value < rhs_value;
        }
        return static_cast<int>(std::get<1>(lhs))
            < static_cast<int>(std::get<1>(rhs));
    });

    std::vector<double> sp_values;
    sp_values.reserve(measure_lines.size() - 1);
    auto active = false;
    auto sp_units = 0.0;
    auto current_sp_measure = SpMeasure {0.0};
    const auto sp_phrase_units = phrase_sp_units(m_sp_engine_values);
    for (const auto& [position, type] : events) {
        if (active) {
            sp_units = clamp_sp_units(
                sp_units - (position - current_sp_measure).value());
        }
        switch (type) {
        case EventType::ActStart:
            active = true;
            break;
        case EventType::ActEnd:
            active = false;
            sp_units = 0.0;
            break;
        case EventType::SpPhrase:
            sp_units = clamp_sp_units(sp_units + sp_phrase_units);
            break;
        case EventType::Measure:
            sp_values.push_back(clamp_sp_units(sp_units) / FULL_SP_UNITS);
            break;
        }
        current_sp_measure = position;
    }
    return sp_values;
}

const std::vector<SightRead::Beat>&
VocalsProcessedSong::activation_starts(std::size_t phrase_index) const
{
    static const std::vector<SightRead::Beat> EMPTY;

    if (phrase_index >= m_activation_starts.size()) {
        return EMPTY;
    }
    return m_activation_starts.at(phrase_index);
}

std::string VocalsProcessedSong::path_summary(const VocalPath& path) const
{
    if (path.activations.empty()) {
        return "No activations";
    }

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2);
    stream << "Acts: ";
    for (std::size_t i = 0; i < path.activations.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        const auto start_measure
            = m_tempo_map.to_measures(path.activations.at(i).start).value();
        stream << start_measure << 'm';
        if (!path.activations.at(i).esf_annotation.empty()) {
            stream << ' ' << path.activations.at(i).esf_annotation;
        }
    }
    return stream.str();
}

VocalPath VocalsOptimiser::optimal_path() const
{
    const auto memo = solve_best_paths(*m_song);
    DpKey key {.phrase_index = 0, .active = false, .sp_bucket = 0};

    struct OpenActivation {
        std::size_t start_phrase_index;
        SightRead::Beat start;
        double sp_start;
        double boosted_pie_fraction;
        double required_prefill_fraction;
        std::string esf_annotation;
        std::optional<std::size_t> last_boosted_phrase;
    };

    VocalPath path;
    path.phrase_score_boosts.resize(m_song->phrases().size(), 0);

    std::optional<OpenActivation> open_activation;
    while (key.phrase_index < m_song->phrases().size()) {
        const auto memo_iter = memo.find(key);
        if (memo_iter == memo.cend()) {
            break;
        }

        std::optional<SightRead::Beat> activation_start;
        if (memo_iter->second.activation_choice.has_value()) {
            const auto& activation_starts
                = m_song->activation_starts(key.phrase_index);
            activation_start
                = activation_starts.at(*memo_iter->second.activation_choice);
            open_activation = OpenActivation {
                .start_phrase_index = key.phrase_index,
                .start = *activation_start,
                .sp_start = from_bucket(key.sp_bucket) / FULL_SP_UNITS,
                .boosted_pie_fraction = 0.0,
                .required_prefill_fraction = 0.0,
                .esf_annotation = "",
                .last_boosted_phrase = std::nullopt};
        }

        const auto result = advance_phrase(*m_song, key, activation_start);
        if (open_activation.has_value() && activation_start.has_value()) {
            open_activation->boosted_pie_fraction = result.boosted_pie_fraction;
            open_activation->required_prefill_fraction
                = result.required_prefill_fraction;
            open_activation->esf_annotation = result.esf_annotation;
        }
        const auto phrase_score_boost = score_points(result.score_gain);
        if (phrase_score_boost > 0) {
            path.phrase_score_boosts.at(key.phrase_index) = phrase_score_boost;
            if (open_activation.has_value()) {
                open_activation->last_boosted_phrase = key.phrase_index;
            }
        }
        if (open_activation.has_value() && result.ended_activation) {
            if (open_activation->last_boosted_phrase.has_value()) {
                path.activations.push_back(
                    {.start_phrase_index = open_activation->start_phrase_index,
                     .end_phrase_index = *open_activation->last_boosted_phrase,
                     .start = open_activation->start,
                     .end = result.activation_end,
                     .sp_start = open_activation->sp_start,
                     .boosted_pie_fraction =
                         open_activation->boosted_pie_fraction,
                     .required_prefill_fraction =
                         open_activation->required_prefill_fraction,
                     .esf_annotation = open_activation->esf_annotation});
            }
            open_activation = std::nullopt;
        }

        key = result.next_key;
    }

    if (open_activation.has_value()
        && open_activation->last_boosted_phrase.has_value()) {
        path.activations.push_back(
            {.start_phrase_index = open_activation->start_phrase_index,
             .end_phrase_index = *open_activation->last_boosted_phrase,
             .start = open_activation->start,
             .end = m_song->phrases().back().end,
             .sp_start = open_activation->sp_start,
             .boosted_pie_fraction = open_activation->boosted_pie_fraction,
             .required_prefill_fraction =
                 open_activation->required_prefill_fraction,
             .esf_annotation = open_activation->esf_annotation});
    }

    path.score_boost = std::accumulate(path.phrase_score_boosts.cbegin(),
                                       path.phrase_score_boosts.cend(), 0);
    return path;
}
