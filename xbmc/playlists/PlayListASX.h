/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "PlayList.h"

#include <iostream>

namespace KODI::PLAYLIST
{
class CPlayListASX : public CPlayList
{
public:
  bool LoadData(std::istream& stream) override;

private:
  bool LoadAsxIniInfo(std::istream& stream);
};
} // namespace KODI::PLAYLIST
