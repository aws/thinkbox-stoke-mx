-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
if StokeGlobalInterface == undefined do --If Stoke was removed
(
	local theMainMenu = menuMan.getMainMenuBar() --find the 3ds Max Main Menu
	local theStokeMenu = menuMan.findMenu "Stoke" --look for the "Stoke" entry
	try(menuMan.unRegisterMenu theStokeMenu)catch() --try to remove it
	menuMan.updateMenuBar() --update the main menu bar
	deleteFile (getSourceFileName()) --delete the script from the startup folder
)
)
