#pragma once

#include "scene_switcher.h"

// All scene headers.
#include "scenes/scene_anniversary.hpp"
#include "scenes/scene_clock.hpp"
#include "scenes/scene_clock_second_ring.hpp"
#include "scenes/scene_concentric_circles.hpp"
#include "scenes/scene_empty.hpp"
#include "scenes/game_of_life.hpp"
#include "scenes/scene_snake.hpp"
#include "scenes/scene_test.hpp"
#include "scenes/scene_weather.hpp"
#include "scenes/scene_weather_forecast.hpp"
#include "scenes/scene_weather_minmax.hpp"

// Registers every scene with the scene switcher, in display order.
//
// To add a scene: include its header above and add one register_scene() line
// here. Instances are function-local statics so they live for the whole program
// and avoid global constructor ordering issues.
inline void register_all_scenes() {
  static ClockScene clock_scene;
  static ClockSceneWithSecondHand clock_second_ring_scene;
  static WeatherScene weather_scene;
  static WeatherForecastScene weather_forecast_scene;
  static WeatherMinMaxScene weather_minmax_scene;
  static AnniversaryScene anniversary_scene;
  static GameOfLifeScene game_of_life_scene;
  static SnakeScene snake_scene;
  static ConcentricCircleScene concentric_circles_scene;
  static SpriteTestScene test_scene;
  static EmptyScene empty_scene;

  register_scene(&clock_scene);
  register_scene(&clock_second_ring_scene);
  register_scene(&weather_scene);
  register_scene(&weather_forecast_scene);
  register_scene(&weather_minmax_scene);
  register_scene(&anniversary_scene);
  register_scene(&game_of_life_scene);
  register_scene(&snake_scene);
  register_scene(&concentric_circles_scene);
  register_scene(&test_scene);
  register_scene(&empty_scene);
}
