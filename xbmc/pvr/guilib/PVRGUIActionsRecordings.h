/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "pvr/IPVRComponent.h"

#include <memory>

class CFileItem;

namespace PVR
{
class CPVRRecording;
class CPVRSettings;

class CPVRGUIActionsRecordings : public IPVRComponent
{
public:
  CPVRGUIActionsRecordings();
  ~CPVRGUIActionsRecordings() override;

  /*!
   * @brief Open a dialog with information for a given recording.
   * @param item containing a recording.
   * @return true on success, false otherwise.
   */
  bool ShowRecordingInfo(const CFileItem& item) const;

  /*!
   * @brief Open the recording settings dialog to edit a recording.
   * @param item containing the recording to edit.
   * @return true on success, false otherwise.
   */
  bool EditRecording(const CFileItem& item) const;

  /*!
   * @brief Check if any recording settings can be edited.
   * @param item containing the recording to edit.
   * @return true on success, false otherwise.
   */
  bool CanEditRecording(const CFileItem& item) const;

  /*!
   * @brief Delete a recording, always showing a confirmation dialog.
   * @param item containing a recording to delete.
   * @return true, if the recording was deleted successfully, false otherwise.
   */
  bool DeleteRecording(const CFileItem& item) const;

  /*!
   * @brief Delete all watched recordings contained in the given folder, always showing a
   * confirmation dialog.
   * @param item containing a recording folder containing the items to delete.
   * @return true, if the recordings were deleted successfully, false otherwise.
   */
  bool DeleteWatchedRecordings(const CFileItem& item) const;

  /*!
   * @brief Delete all recordings from trash, always showing a confirmation dialog.
   * @return true, if the recordings were permanently deleted successfully, false otherwise.
   */
  bool DeleteAllRecordingsFromTrash() const;

  /*!
   * @brief Undelete a recording.
   * @param item containing a recording to undelete.
   * @return true, if the recording was undeleted successfully, false otherwise.
   */
  bool UndeleteRecording(const CFileItem& item) const;

  /*!
   * @brief Increment the play count of a recording. Process "Delete after watching" action.
   * @param item containing a recording for which the play count shall be incremented.
   * @return true, if the recording's play count was incremented successfully, false otherwise.
   */
  bool IncrementPlayCount(const CFileItem& item) const;

  /*!
   * @brief Mark a recording watched or unwatched. Process "Delete after watching" action.
   * @param item containing a recording to be marked watched or unwatched.
   * @param watched Whether to mark the recording watched or unwatched.
   * @return true, if the recording's watched state was changed successfully, false otherwise.
   */
  bool MarkWatched(const CFileItem& item, bool watched) const;

private:
  CPVRGUIActionsRecordings(const CPVRGUIActionsRecordings&) = delete;
  CPVRGUIActionsRecordings const& operator=(CPVRGUIActionsRecordings const&) = delete;

  /*!
   * @brief Open a dialog to confirm to delete a recording.
   * @param item the recording to delete.
   * @return true, to proceed with delete, false otherwise.
   */
  bool ConfirmDeleteRecording(const CFileItem& item) const;

  /*!
   * @brief Open a dialog to confirm delete all watched recordings contained in the given folder.
   * @param item containing a recording folder containing the items to delete.
   * @return true, to proceed with delete, false otherwise.
   */
  bool ConfirmDeleteWatchedRecordings(const CFileItem& item) const;

  /*!
   * @brief Open a dialog to confirm to permanently remove all deleted recordings.
     * @return true, to proceed with delete, false otherwise.
   */
  bool ConfirmDeleteAllRecordingsFromTrash() const;

  /*!
   * @brief Open the recording settings dialog.
   * @param recording containing the recording the settings shall be displayed for.
   * @return true, if the dialog was ended successfully, false otherwise.
   */
  bool ShowRecordingSettings(const std::shared_ptr<CPVRRecording>& recording) const;

  /*!
   * @brief Process action according to "Delete after watching" setting value.
   * @param item The watched recording.
   * @return true on success, false otherwise.
   */
  bool ProcessDeleteAfterWatch(const CFileItem& item) const;

  std::unique_ptr<CPVRSettings> m_settings;
};

namespace GUI
{
// pretty scope and name
using Recordings = CPVRGUIActionsRecordings;
} // namespace GUI

} // namespace PVR
