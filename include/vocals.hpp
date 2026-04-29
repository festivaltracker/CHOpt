#ifndef CHOPT_VOCALS_HPP
#define CHOPT_VOCALS_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <sightread/songparts.hpp>
#include <sightread/tempomap.hpp>

#include "settings.hpp"
#include "sptimemap.hpp"

struct VocalPieProfile {
    double duration_weight_per_beat;
    double tube_weight_beats;
    double perfect_vibrato_multiplier;
    double nonpitch_multiplier;
    double short_tube_threshold_ms;
    double short_tube_multiplier;

    [[nodiscard]] static VocalPieProfile default_profile();
    [[nodiscard]] static VocalPieProfile rb3_default();
};

struct VocalPieSegment {
    SightRead::Beat start;
    SightRead::Beat end;
    double normal_units;
    double perfect_units;
};

struct VocalPhrasePie {
    double target_units {0.0};
    std::vector<VocalPieSegment> segments;

    [[nodiscard]] double raw_units_between(SightRead::Beat start,
                                           SightRead::Beat end,
                                           bool perfect_vibrato) const;
    [[nodiscard]] double fill_between(SightRead::Beat start, SightRead::Beat end,
                                      bool perfect_vibrato = true) const;
    [[nodiscard]] double suffix_capacity(SightRead::Beat start) const;
    [[nodiscard]] double required_prefill(SightRead::Beat start) const;
    [[nodiscard]] double boosted_fraction(SightRead::Beat active_start,
                                          SightRead::Beat active_end) const;
};

struct VocalActivationWindow {
    std::size_t target_phrase_index;
    SightRead::Beat start;
    SightRead::Beat end;
};

struct VocalActivation {
    std::size_t start_phrase_index;
    std::size_t end_phrase_index;
    SightRead::Beat start;
    SightRead::Beat end;
    double sp_start;
    double boosted_pie_fraction {0.0};
    double required_prefill_fraction {0.0};
    std::string esf_annotation;
};

struct VocalPath {
    std::vector<VocalActivation> activations;
    std::vector<int> phrase_score_boosts;
    int score_boost {0};
};

struct VocalScoredSegment {
    SightRead::Beat start;
    SightRead::Beat end;
    double duration_seconds;
};

struct VocalPhraseInfo {
    SightRead::Beat start;
    SightRead::Beat end;
    SpMeasure start_sp;
    SpMeasure end_sp;
    bool is_sp_phrase;
    int multiplier;
    int base_score;
    double total_scored_seconds;
    std::vector<VocalScoredSegment> scored_segments;
    VocalPhrasePie pie;
};

class VocalsProcessedSong {
private:
    SightRead::TempoMap m_tempo_map;
    SpTimeMap m_time_map;
    SpEngineValues m_sp_engine_values;
    std::vector<VocalPhraseInfo> m_phrases;
    std::vector<VocalActivationWindow> m_activation_windows;
    std::vector<std::vector<SightRead::Beat>> m_activation_starts;
    int m_total_base_score {0};

public:
    VocalsProcessedSong(const SightRead::VocalTrack& track,
                        const PathingSettings& pathing_settings);

    [[nodiscard]] const std::vector<VocalPhraseInfo>& phrases() const
    {
        return m_phrases;
    }
    [[nodiscard]] const std::vector<VocalActivationWindow>&
    activation_windows() const
    {
        return m_activation_windows;
    }
    [[nodiscard]] const std::vector<SightRead::Beat>&
    activation_starts(std::size_t phrase_index) const;
    [[nodiscard]] const SightRead::TempoMap& tempo_map() const
    {
        return m_tempo_map;
    }
    [[nodiscard]] const SpTimeMap& time_map() const { return m_time_map; }
    [[nodiscard]] const SpEngineValues& sp_engine_values() const
    {
        return m_sp_engine_values;
    }
    [[nodiscard]] int total_base_score() const { return m_total_base_score; }

    [[nodiscard]] std::vector<int>
    base_score_values(const std::vector<double>& measure_lines) const;
    [[nodiscard]] std::vector<int>
    running_score_values(const std::vector<double>& measure_lines,
                         const VocalPath& path) const;
    [[nodiscard]] std::vector<double>
    sp_percent_values(const std::vector<double>& measure_lines,
                      const VocalPath& path) const;
    [[nodiscard]] std::string path_summary(const VocalPath& path) const;
};

class VocalsOptimiser {
private:
    const VocalsProcessedSong* m_song;

public:
    explicit VocalsOptimiser(const VocalsProcessedSong* song)
        : m_song {song}
    {
    }

    [[nodiscard]] VocalPath optimal_path() const;
};

#endif
