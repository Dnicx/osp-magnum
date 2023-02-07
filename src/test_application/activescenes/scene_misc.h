/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "scenarios.h"

namespace testapp::scenes
{

void add_floor(
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scnCommon,
        osp::Session const&         material,
        osp::Session const&         shapeSpawn,
        osp::TopDataId              idResources,
        osp::PkgId                  pkg);

/**
 * @brief Create CameraController connected to an app's UserInputHandler
 */
osp::Session setup_camera_ctrl(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         app,
        osp::Session const&         scnRender);

/**
 * @brief Adds free cam controls to a CameraController
 */
osp::Session setup_camera_free(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         app,
        osp::Session const&         scnCommon,
        osp::Session const&         camera);

/**
 * @brief Throws spheres when pressing space
 */
osp::Session setup_thrower(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         renderer,
        osp::Session const&         simpleCamera,
        osp::Session const&         shapeSpawn);

/**
 * @brief Spawn blocks every 2 seconds and spheres every 1 second
 */
osp::Session setup_droppers(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         shapeSpawn);

}