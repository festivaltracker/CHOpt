#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "test_helpers.hpp"
#include "vocals.hpp"

namespace {
SightRead::VocalTrack
make_vocal_track(std::vector<SightRead::VocalPhrase> phrases,
                 std::vector<SightRead::VocalTube> tubes = {},
                 std::vector<SightRead::LyricEvent> lyrics = {})
{
    return {std::move(tubes), std::move(lyrics), std::move(phrases),
            std::make_shared<SightRead::SongGlobalData>()};
}

SightRead::VocalTrack
make_vocal_track_at_bpm(double bpm, std::vector<SightRead::VocalPhrase> phrases,
                        std::vector<SightRead::VocalTube> tubes = {},
                        std::vector<SightRead::LyricEvent> lyrics = {})
{
    auto global_data = std::make_shared<SightRead::SongGlobalData>();
    global_data->tempo_map(
        SightRead::TempoMap {{},
                             {{.position = SightRead::Tick {0},
                               .millibeats_per_minute = bpm * 1000.0}},
                             {},
                             192});
    return {std::move(tubes), std::move(lyrics), std::move(phrases),
            std::move(global_data)};
}

SightRead::Tick measure_tick(double measure)
{
    constexpr auto TICKS_PER_MEASURE = 768.0;
    return SightRead::Tick {
        static_cast<int>(std::lround(measure * TICKS_PER_MEASURE))};
}

SightRead::Tick beat_tick(double beat)
{
    constexpr auto TICKS_PER_BEAT = 192.0;
    return SightRead::Tick {
        static_cast<int>(std::lround(beat * TICKS_PER_BEAT))};
}
}

BOOST_AUTO_TEST_SUITE(vocals_processed_song)

BOOST_AUTO_TEST_CASE(phrase_multiplier_increases_and_caps_at_four_x)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1536},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {96},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1536},
                             .length = SightRead::Tick {96},
                             .pitch = 67,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.phrases().size(), 5U);
    BOOST_CHECK_EQUAL(song.phrases().at(0).multiplier, 1);
    BOOST_CHECK_EQUAL(song.phrases().at(1).multiplier, 2);
    BOOST_CHECK_EQUAL(song.phrases().at(2).multiplier, 3);
    BOOST_CHECK_EQUAL(song.phrases().at(3).multiplier, 4);
    BOOST_CHECK_EQUAL(song.phrases().at(4).multiplier, 4);
}

BOOST_AUTO_TEST_CASE(rb_phrase_base_scores_follow_combo_multiplier)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1536},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {96},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1536},
                             .length = SightRead::Tick {96},
                             .pitch = 67,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.phrases().size(), 5U);
    BOOST_CHECK_EQUAL(song.phrases().at(0).base_score, 1000);
    BOOST_CHECK_EQUAL(song.phrases().at(1).base_score, 2000);
    BOOST_CHECK_EQUAL(song.phrases().at(2).base_score, 3000);
    BOOST_CHECK_EQUAL(song.phrases().at(3).base_score, 4000);
    BOOST_CHECK_EQUAL(song.phrases().at(4).base_score, 4000);
    BOOST_CHECK_EQUAL(song.total_base_score(), 14000);
}

BOOST_AUTO_TEST_CASE(fortnite_karaoke_phrase_base_scores_ignore_tube_count)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {480},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {64},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {832},
                             .length = SightRead::Tick {64},
                             .pitch = 67,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {896},
                             .length = SightRead::Tick {64},
                             .pitch = 69,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.phrases().size(), 3U);
    BOOST_CHECK_EQUAL(song.phrases().at(0).base_score, 1000);
    BOOST_CHECK_EQUAL(song.phrases().at(1).base_score, 2000);
    BOOST_CHECK_EQUAL(song.phrases().at(2).base_score, 3000);
    BOOST_CHECK_EQUAL(song.total_base_score(), 6000);
}

BOOST_AUTO_TEST_CASE(longer_tubes_have_more_pie_units)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.phrases().size(), 2U);
    BOOST_CHECK_GT(song.phrases().at(1).pie.target_units,
                   song.phrases().at(0).pie.target_units);
}

BOOST_AUTO_TEST_CASE(more_tubes_with_same_total_duration_have_more_pie_units)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {480},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.phrases().size(), 2U);
    BOOST_CHECK_CLOSE(song.phrases().at(0).pie.target_units, 1.25, 0.0001);
    BOOST_CHECK_CLOSE(song.phrases().at(1).pie.target_units, 1.5, 0.0001);
}

BOOST_AUTO_TEST_CASE(perfect_vibrato_fills_pitched_tubes_faster)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};

    const auto& phrase = song.phrases().front();
    const auto first_half_end = SightRead::Beat {0.5};
    BOOST_CHECK_GT(
        phrase.pie.fill_between(phrase.start, first_half_end, true),
        phrase.pie.fill_between(phrase.start, first_half_end, false));
}

BOOST_AUTO_TEST_CASE(activation_windows_are_generated_from_phrase_content_gaps)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {420},
                             .length = SightRead::Tick {60},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Unpitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 2U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 0.5,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 2.1875,
                      0.0001);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(1).start.value(), 2.5,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(1).end.value(), 4.0, 0.0001);
    BOOST_CHECK_EQUAL(song.activation_windows().at(1).target_phrase_index, 2U);
}

BOOST_AUTO_TEST_CASE(short_inter_phrase_gaps_do_not_create_activation_windows)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {240},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {240},
                             .length = SightRead::Tick {192},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_CHECK(song.activation_windows().empty());
}

BOOST_AUTO_TEST_CASE(vocal_path_summary_reports_skipped_activation_windows)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {420},
                             .length = SightRead::Tick {60},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Unpitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalPath path {
        .activations = {{.start_phrase_index = 2,
                         .end_phrase_index = 2,
                         .start = song.activation_windows().at(1).start,
                         .end = song.phrases().at(2).end,
                         .sp_start = 0.25}},
        .phrase_score_boosts = {0, 0, song.phrases().at(2).base_score},
        .score_boost = song.phrases().at(2).base_score};

    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 1/s1");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 1/sk1");
}

BOOST_AUTO_TEST_CASE(
    vocal_path_summary_counts_sp_phrases_after_the_previous_activation)
{
    const auto track = make_vocal_track({{.position = SightRead::Tick {0},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {192},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {384},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = false},
                                         {.position = SightRead::Tick {576},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {768},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {960},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {1152},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {1344},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = false}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};
    const VocalPath path {.activations = {{.start_phrase_index = 2,
                                           .end_phrase_index = 3,
                                           .start = song.phrases().at(2).start,
                                           .end = song.phrases().at(3).end,
                                           .sp_start = 0.5},
                                          {.start_phrase_index = 7,
                                           .end_phrase_index = 7,
                                           .start = song.phrases().at(7).start,
                                           .end = song.phrases().at(7).end,
                                           .sp_start = 0.75}},
                          .phrase_score_boosts = {0, 0, 0, 0, 0, 0, 0, 0},
                          .score_boost = 0};

    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 2/ 3/");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 2/ 3/");
}

BOOST_AUTO_TEST_CASE(vocal_path_summary_uses_sp_available_at_activation_start)
{
    const auto track = make_vocal_track({{.position = SightRead::Tick {0},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {192},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {384},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = false},
                                         {.position = SightRead::Tick {576},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {768},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = true},
                                         {.position = SightRead::Tick {960},
                                          .length = SightRead::Tick {192},
                                          .is_sp_phrase = false}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalPath path {.activations = {{.start_phrase_index = 2,
                                           .end_phrase_index = 3,
                                           .start = song.phrases().at(2).start,
                                           .end = song.phrases().at(3).end,
                                           .sp_start = 0.5},
                                          {.start_phrase_index = 5,
                                           .end_phrase_index = 5,
                                           .start = song.phrases().at(5).start,
                                           .end = song.phrases().at(5).end,
                                           .sp_start = 0.5}},
                          .phrase_score_boosts = {0, 0, 0, 0, 0, 0},
                          .score_boost = 0};

    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 2/ 2/");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 2/ 2/");
}

BOOST_AUTO_TEST_CASE(vocal_path_summary_reports_before_od_activations)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {420},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {804},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1200},
                             .length = SightRead::Tick {96},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const auto before_od_window = std::ranges::find_if(
        song.activation_windows(),
        [&](const auto& window) { return window.target_phrase_index == 3U; });
    BOOST_REQUIRE(before_od_window != song.activation_windows().cend());
    const VocalPath path {.activations = {{.start_phrase_index = 3,
                                           .end_phrase_index = 3,
                                           .start = before_od_window->start,
                                           .end = song.phrases().at(3).end,
                                           .sp_start = 0.25}},
                          .phrase_score_boosts = {0, 0, 0, 0},
                          .score_boost = 0};

    BOOST_CHECK_LT(path.activations.at(0).start.value(),
                   song.phrases().at(3).start.value());
    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 1/B");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 1/BOD");
}

BOOST_AUTO_TEST_CASE(
    multiple_internal_phrase_gaps_create_multiple_windows_for_one_phrase)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {1104},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {432},
                             .length = SightRead::Tick {192},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {864},
                             .length = SightRead::Tick {192},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1296},
                             .length = SightRead::Tick {192},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 3U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 1.0,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 2.25,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(1).start.value(), 3.25,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(1).end.value(), 4.5, 0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(2).start.value(), 5.5,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(2).end.value(), 6.75,
                      0.0001);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(1).target_phrase_index, 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(2).target_phrase_index, 1U);
}

BOOST_AUTO_TEST_CASE(one_sp_phrase_is_enough_to_start_an_activation)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {48},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).start_phrase_index, 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).end_phrase_index, 1U);
    BOOST_CHECK_GT(path.activations.at(0).start.value(), 1.99);
    BOOST_CHECK_LT(path.activations.at(0).start.value(), 2.0);
    BOOST_CHECK_GE(path.activations.at(0).sp_start, 0.25);
    BOOST_CHECK_GT(path.score_boost, 0);
    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 1/");
}

BOOST_AUTO_TEST_CASE(fortnite_karaoke_squeeze_limits_late_activation_starts)
{
    auto settings = default_karaoke_pathing_settings();
    settings.squeeze = 0.5;
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {768},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {384},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, settings};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 1.0,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 4.0, 0.0001);

    const auto& starts = song.activation_starts(1);
    BOOST_REQUIRE_EQUAL(starts.size(), 2U);
    BOOST_CHECK_CLOSE(starts.at(0).value(), 1.0, 0.0001);
    BOOST_CHECK_CLOSE(starts.at(1).value(), 2.5, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    activation_before_first_tube_after_phrase_start_is_not_an_esf)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {384},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .pitch = 61,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {816},
                             .length = SightRead::Tick {336},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_GT(path.activations.at(0).start.value(),
                   song.phrases().at(2).start.value());
    BOOST_CHECK_LT(path.activations.at(0).start.value(),
                   song.phrases().at(2).scored_segments.front().start.value());
    BOOST_CHECK(path.activations.at(0).esf_annotation.empty());
    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 2/");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 2/");
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_full_esf_boosts_the_entire_phrase_under_od)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {768},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {1},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {384},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).esf_annotation, "S");
    BOOST_CHECK_CLOSE(path.activations.at(0).boosted_pie_fraction, 1.0, 0.0001);
    BOOST_CHECK_CLOSE(path.activations.at(0).required_prefill_fraction, 0.0,
                      0.0001);
    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 2U);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(1),
                      song.phrases().at(1).base_score);
    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 1/S");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 1/ESF");
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_mid_phrase_activation_does_not_refill_active_sp_phrase)
{
    const auto track = make_vocal_track_at_bpm(
        300.0,
        {{.position = beat_tick(0.0),
          .length = beat_tick(4.0),
          .is_sp_phrase = true},
         {.position = beat_tick(4.0),
          .length = beat_tick(8.0),
          .is_sp_phrase = true},
         {.position = beat_tick(30.5),
          .length = beat_tick(2.0),
          .is_sp_phrase = false}},
        {{.position = beat_tick(0.0),
          .length = beat_tick(4.0),
          .pitch = 60,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = beat_tick(4.0),
          .length = beat_tick(0.1),
          .pitch = 62,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = beat_tick(10.0),
          .length = beat_tick(2.0),
          .pitch = 64,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = beat_tick(30.5),
          .length = beat_tick(2.0),
          .pitch = 65,
          .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE(song.active_sp_pickup_requires_phrase_start());
    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_GT(path.activations.at(0).start.value(),
                   song.phrases().at(1).start.value());
    BOOST_CHECK_EQUAL(path.activations.at(0).end_phrase_index, 1U);
    BOOST_CHECK_LT(path.activations.at(0).end.value(),
                   song.phrases().at(2).start.value());
    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 3U);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(2), 0);
}

BOOST_AUTO_TEST_CASE(fortnite_karaoke_uses_standard_od_drain)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong karaoke_song {track,
                                            default_karaoke_pathing_settings()};
    const VocalsProcessedSong rb_song {track,
                                       default_rb_vocals_pathing_settings()};

    BOOST_CHECK_CLOSE(karaoke_song.sp_drain_rate(), 1.0, 0.0001);
    BOOST_CHECK_CLOSE(rb_song.sp_drain_rate(), 1.0, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_prefill_esf_reports_required_prefill_and_partial_boost)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {240},
                             .length = SightRead::Tick {1152},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {240},
                             .length = SightRead::Tick {714},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1200},
                             .length = SightRead::Tick {192},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).esf_annotation, "S60");
    BOOST_CHECK_CLOSE(path.activations.at(0).boosted_pie_fraction, 0.4, 0.0001);
    BOOST_CHECK_CLOSE(path.activations.at(0).required_prefill_fraction, 0.6,
                      0.0001);
    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 2U);
    BOOST_CHECK_EQUAL(
        path.phrase_score_boosts.at(1),
        static_cast<int>(std::lround(song.phrases().at(1).base_score * 0.4)));
    BOOST_CHECK_EQUAL(song.path_summary(path), "Acts: 1/S60");
    BOOST_CHECK_EQUAL(song.path_summary(path, VocalPathNotation::ScoreHero),
                      "Acts: 1/ESP");
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_activation_ending_mid_phrase_only_boosts_active_pie_fill)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {4608},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {1},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {3840},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 2U);
    BOOST_CHECK_LT(path.activations.at(0).boosted_pie_fraction, 1.0);
    BOOST_CHECK_GT(path.activations.at(0).boosted_pie_fraction, 0.0);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(1),
                      static_cast<int>(std::lround(
                          song.phrases().at(1).base_score
                          * path.activations.at(0).boosted_pie_fraction)));
}

BOOST_AUTO_TEST_CASE(
    later_internal_windows_allow_partial_phrase_boosts_and_future_carry)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {360},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {624},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {1008},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {816},
                             .length = SightRead::Tick {192},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1008},
                             .length = SightRead::Tick {192},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).start_phrase_index, 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).end_phrase_index, 2U);
    BOOST_CHECK_GT(path.activations.at(0).start.value(), 4.24);
    BOOST_CHECK_LT(path.activations.at(0).start.value(), 4.25);
    BOOST_CHECK_CLOSE(path.activations.at(0).end.value(), 6.25, 0.0001);

    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 3U);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(0), 0);
    BOOST_CHECK_GT(path.activations.at(0).boosted_pie_fraction, 0.0);
    BOOST_CHECK_LT(path.activations.at(0).boosted_pie_fraction, 1.0);
    const auto first_boost = static_cast<int>(
        std::lround(song.phrases().at(1).base_score
                    * path.activations.at(0).boosted_pie_fraction));
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(1), first_boost);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(2),
                      song.phrases().at(2).base_score);
    BOOST_CHECK_EQUAL(path.score_boost,
                      first_boost + song.phrases().at(2).base_score);
}

BOOST_AUTO_TEST_CASE(small_internal_phrase_gaps_do_not_create_windows)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {632},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {392},
                             .length = SightRead::Tick {240},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_CHECK(song.activation_windows().empty());
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_phrase_boundary_gaps_use_full_tube_to_tube_duration)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {240},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {240},
                             .length = SightRead::Tick {384},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {432},
                             .length = SightRead::Tick {192},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 1.0,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 2.25,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_shows_three_and_half_measure_post_phrase_windows)
{
    const auto track
        = make_vocal_track({{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false},
                            {.position = measure_tick(4.5),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false}},
                           {{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(4.5),
                             .length = measure_tick(0.5),
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 4.0,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 18.0,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_large_post_phrase_gaps_reappear_before_next_phrase)
{
    const auto track
        = make_vocal_track({{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false},
                            {.position = measure_tick(4.75),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false}},
                           {{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(4.75),
                             .length = measure_tick(0.5),
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_CLOSE(song.tempo_map()
                          .to_measures(song.activation_windows().at(0).start)
                          .value(),
                      3.8911, 0.01);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(),
                      measure_tick(4.75).value() / 192.0, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_long_post_phrase_gaps_reappear_near_next_phrase)
{
    const auto track = make_vocal_track_at_bpm(
        97.0,
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .is_sp_phrase = false},
         {.position = measure_tick(20.75),
          .length = measure_tick(1.0),
          .is_sp_phrase = false}},
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .pitch = 60,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = measure_tick(20.75),
          .length = measure_tick(0.5),
          .pitch = 62,
          .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_CLOSE(song.tempo_map()
                          .to_measures(song.activation_windows().at(0).start)
                          .value(),
                      19.6875, 0.01);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(),
                      measure_tick(20.75).value() / 192.0, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_very_fast_long_post_phrase_gaps_can_stay_hidden)
{
    const auto track = make_vocal_track_at_bpm(
        300.0,
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .is_sp_phrase = false},
         {.position = measure_tick(20.75),
          .length = measure_tick(1.0),
          .is_sp_phrase = false}},
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .pitch = 60,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = measure_tick(20.75),
          .length = measure_tick(0.5),
          .pitch = 62,
          .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_CHECK(song.activation_windows().empty());
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_faster_long_post_phrase_gaps_reappear_window_shrinks)
{
    const auto track = make_vocal_track_at_bpm(
        120.0,
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .is_sp_phrase = false},
         {.position = measure_tick(20.75),
          .length = measure_tick(1.0),
          .is_sp_phrase = false}},
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .pitch = 60,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = measure_tick(20.75),
          .length = measure_tick(0.5),
          .pitch = 62,
          .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_CLOSE(song.tempo_map()
                          .to_measures(song.activation_windows().at(0).start)
                          .value(),
                      19.8911, 0.01);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(),
                      measure_tick(20.75).value() / 192.0, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_slower_long_post_phrase_gaps_reappear_window_grows)
{
    const auto track = make_vocal_track_at_bpm(
        80.0,
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .is_sp_phrase = false},
         {.position = measure_tick(20.75),
          .length = measure_tick(1.0),
          .is_sp_phrase = false}},
        {{.position = measure_tick(0.0),
          .length = measure_tick(1.0),
          .pitch = 60,
          .type = SightRead::VocalTubeType::Pitched},
         {.position = measure_tick(20.75),
          .length = measure_tick(0.5),
          .pitch = 62,
          .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_CLOSE(song.tempo_map()
                          .to_measures(song.activation_windows().at(0).start)
                          .value(),
                      19.4617, 0.01);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(),
                      measure_tick(20.75).value() / 192.0, 0.0001);
}

BOOST_AUTO_TEST_CASE(
    fortnite_karaoke_uses_phrase_start_for_post_phrase_window_threshold)
{
    const auto track
        = make_vocal_track({{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false},
                            {.position = measure_tick(4.25),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = false}},
                           {{.position = measure_tick(0.0),
                             .length = measure_tick(1.0),
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(4.75),
                             .length = measure_tick(0.5),
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_EQUAL(song.activation_windows().at(0).target_phrase_index, 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 4.0,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 19.0,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(large_internal_phrase_gaps_create_windows)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {960},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {240},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {480},
                             .length = SightRead::Tick {480},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track, default_karaoke_pathing_settings()};

    BOOST_REQUIRE_EQUAL(song.activation_windows().size(), 1U);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).start.value(), 1.25,
                      0.0001);
    BOOST_CHECK_CLOSE(song.activation_windows().at(0).end.value(), 2.5, 0.0001);
}

BOOST_AUTO_TEST_CASE(rb_vocals_require_two_sp_phrases_to_activate)
{
    const auto track
        = make_vocal_track({{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = true},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {192},
                             .is_sp_phrase = false}},
                           {{.position = SightRead::Tick {0},
                             .length = SightRead::Tick {96},
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {384},
                             .length = SightRead::Tick {96},
                             .pitch = 62,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {768},
                             .length = SightRead::Tick {96},
                             .pitch = 64,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = SightRead::Tick {1152},
                             .length = SightRead::Tick {96},
                             .pitch = 65,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_EQUAL(path.activations.size(), 1U);
    BOOST_CHECK_EQUAL(path.activations.at(0).start_phrase_index, 3U);
    BOOST_CHECK_GE(path.activations.at(0).sp_start, 0.5);
}

BOOST_AUTO_TEST_CASE(rb_vocals_late_esf_can_full_boost_two_phrases)
{
    const auto track
        = make_vocal_track({{.position = measure_tick(0.0),
                             .length = measure_tick(0.5),
                             .is_sp_phrase = true},
                            {.position = measure_tick(1.0),
                             .length = measure_tick(0.5),
                             .is_sp_phrase = true},
                            {.position = measure_tick(4.0),
                             .length = measure_tick(2.625),
                             .is_sp_phrase = false},
                            {.position = measure_tick(7.75),
                             .length = measure_tick(2.25),
                             .is_sp_phrase = true},
                            {.position = measure_tick(12.0),
                             .length = measure_tick(1.0),
                             .is_sp_phrase = true},
                            {.position = measure_tick(16.5),
                             .length = measure_tick(4.0),
                             .is_sp_phrase = false},
                            {.position = measure_tick(20.625),
                             .length = measure_tick(1.5),
                             .is_sp_phrase = false}},
                           {{.position = measure_tick(0.0),
                             .length = measure_tick(0.25),
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(1.0),
                             .length = measure_tick(0.25),
                             .pitch = 60,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(4.1),
                             .length = measure_tick(2.3),
                             .pitch = 68,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(7.9),
                             .length = measure_tick(1.6),
                             .pitch = 68,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(12.1),
                             .length = measure_tick(0.7),
                             .pitch = 68,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(16.679),
                             .length = measure_tick(0.259),
                             .pitch = 73,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(16.985),
                             .length = measure_tick(0.653),
                             .pitch = 61,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(18.670),
                             .length = measure_tick(0.221),
                             .pitch = 73,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(18.963),
                             .length = measure_tick(1.218),
                             .pitch = 73,
                             .type = SightRead::VocalTubeType::Pitched},
                            {.position = measure_tick(20.677),
                             .length = measure_tick(0.941),
                             .pitch = 73,
                             .type = SightRead::VocalTubeType::Pitched}});
    const VocalsProcessedSong song {track,
                                    default_rb_vocals_pathing_settings()};
    const VocalsOptimiser optimiser {&song};
    const auto path = optimiser.optimal_path();

    BOOST_REQUIRE_GE(path.activations.size(), 2U);
    const auto& late_activation = path.activations.back();
    BOOST_CHECK_EQUAL(late_activation.start_phrase_index, 5U);
    BOOST_CHECK_EQUAL(late_activation.end_phrase_index, 6U);
    BOOST_CHECK_CLOSE(
        song.tempo_map().to_measures(late_activation.start).value(), 18.67,
        0.01);
    BOOST_CHECK_EQUAL(late_activation.esf_annotation, "S");
    BOOST_REQUIRE_EQUAL(path.phrase_score_boosts.size(), 7U);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(5),
                      song.phrases().at(5).base_score);
    BOOST_CHECK_EQUAL(path.phrase_score_boosts.at(6),
                      song.phrases().at(6).base_score);
}

BOOST_AUTO_TEST_SUITE_END()
