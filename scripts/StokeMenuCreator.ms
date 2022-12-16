-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
	local CurrentVersion = "20190619" --when the content of the Menu definition changes, this value should be updated (typically to the current date) to force a refresh of existing Stoke menus.
	local isMenuRequested = true --not matchPattern (getIniSetting (GetDir #plugcfg + "\\StokePreferences.ini" ) "MenuSystem" "CreateMenu") pattern:"false"
	local theMainMenu = menuMan.getMainMenuBar()																--get the Main Menu
	local theStokeMenu = menuMan.findMenu "Stoke"																--find an existing menu, if any
	local oldVersion = (getIniSetting (GetDir #plugcfg + "\\StokePreferences.ini" ) "MenuSystem" "LastVersion")	--get the previously stored version of the currently installed menu
	if oldVersion == "" do oldVersion = "20130101"																--if it is not defined yet, assume an ancient date before Stoke 1.0 release
	local needUpdate = oldVersion != CurrentVersion																--if the versions are different, we need to rebuild the menu
	if theStokeMenu != undefined and (not isMenuRequested or needUpdate) do 									--if the menu exists, and an update is requested, we will update it.
	(
		if needUpdate then
			format "-->Old Stoke Menu Version % Found and Removed.\n" oldVersion
		else	
			format "-->Stoke Menu Found and Removed at User Request via the Stoke Preferences.\n"
		
		while theStokeMenu != undefined do
		(
			menuMan.unRegisterMenu theStokeMenu
			menuMan.updateMenuBar() 
			theStokeMenu = menuMan.findMenu "Stoke"
		)
		theStokeMenu = undefined
	)
	if theStokeMenu == undefined and isMenuRequested do
	(
		if needUpdate then
			format "-->Creating Updated Stoke Menu Version %...\n" CurrentVersion
		else
			format "-->Stoke Menu Not Found - Creating...\n"
		
		theStokeMenu = menuMan.createMenu "Stoke"

		theMenuDefinitionArray = #(
			#(#menuitem,"Stoke","Stoke",						"Create a STOKE Particle Reflow Simulator - hold SHIFT to create at World Origin"),
			#(#separator),
			#(#menuitem,"StokePartitioning","Stoke",			"Open the STOKE Particle PARTITIONING dialog..."),
			#(#menuitem,"StokeFieldSimOnDeadline","Stoke",		"Open the Field Simulation DEADLINE SUBMITTER dialog..."),
			#(#separator),
			#(#menuitem,"CreateStokeFieldLoader","Stoke",		"Create a Field LOADER object - hold SHIFT to create at World Origin"),
			#(#menuitem,"CreateStokeField","Stoke",				"Create a Field MAGMA object - hold SHIFT to create at World Origin"),
			#(#menuitem,"CreateStokeFieldSim","Stoke",			"Create a Field SIMULATOR object - hold SHIFT to create at World Origin"),
			#(#menuitem,"CreateStokePRTField","Stoke",			"Create a PRT FIELD object - hold SHIFT to create at World Origin"),
			#(#menuitem,"CreateStokeFieldMesher","Stoke",		"Create a Field MESHER object - hold SHIFT to create at World Origin"),
			#(#menuitem,"CreateStokeFieldForce","Stoke",		"Create a Field FORCE SpaceWarp object - hold SHIFT to create at World Origin"),
			#(#separator),
			#(#menuitem,"AddStokeFieldFromMesh", "Stoke", 		"Add a MESH TO FIELD Modifier to selected object(s)"),
			#(#menuitem,"AddStokeFieldDisplayModifier","Stoke", "Add a Field DISPLAY Modifier to selected object(s)"),
			#(#menuitem,"AddStokeFieldFluid","Stoke", 			"Add a Field FLUID MOTION Modifier to selected object(s)"),
			#(#separator),
			#(#submenustart,"GENOME Modifiers..."),
			#(#menuitem,"AddGenomeModifier","Genome", 			"Add a GENOME Modifier to selected geometry object(s)"),
			#(#menuitem,"AddGenomePreset","Genome", 			"Add a GENOME PRESET Modifier to selected geometry object(s)..."),
			#(#submenuend),
			#(#separator),
			#(#menuitem,"OpenStokeFieldDataViewer","Stoke", 	"Open a new Stoke FIELD DATA VIEWER Spreadsheet"),
			#(#menuitem,"StokeFieldProbe","Stoke", 				"Open the Stoke Field PROBE tool"),
			#(#separator),
			#(#menuitem,"StokeFieldDataExporter","Stoke", 		"Open the FIELD DATA EXPORTER Dialog - save to FumeFX FXD, Field3D F3D, or OpenVDB VDB files"),
			#(#menuitem,"Stoke2FGAExporter","Stoke", 			"Open the FIELD To FGA EXPORTER Dialog - save a Stoke Vector Field to a Field Grid ASCII (FGA) file"),
			#(#separator),
			#(#menuitem,"StokeLogLevelNone","Stoke", 			"Set Stoke LOG Level To NONE"),
			#(#menuitem,"StokeLogLevelWarning","Stoke", 		"Set Stoke LOG Level To WARNING"),
			#(#separator),
			#(#menuitem,"AboutStoke","Stoke", 					"ABOUT Stoke...")
		)

		local theSubMenu = undefined
		for aDef in theMenuDefinitionArray do
		(
			case aDef[1] of
			(
				#menuitem:
				(
					try(
						theAction = menuMan.createActionItem aDef[2] aDef[3]
						theAction.setTitle aDef[4]
						theAction.setUseCustomTitle true
						if theSubMenu == undefined then
							theStokeMenu.addItem theAction -1
						else
							theSubMenu.addItem theAction -1
					)catch()
				)
				#separator: 
				(
					try(
						theAction = menuMan.createSeparatorItem()
						if theSubMenu == undefined then
							theStokeMenu.addItem theAction -1
						else
							theSubMenu.addItem theAction -1
					)catch()
				)
				#submenustart:
				(
					try(
						theSubMenu = menuMan.createMenu aDef[2]
					)catch()
				)
				#submenuend: 
				(
					try(
						theAction = menuMan.createSubMenuItem (theSubMenu.getTitle()) theSubMenu
						theStokeMenu.addItem theAction -1	
					)catch()
					theSubMenu = undefined
				)
			)--end case
		)--end aDef loop
		theSubMenu = menuMan.createSubMenuItem "Stoke" theStokeMenu
		theMainMenu.addItem theSubMenu -1
		menuMan.updateMenuBar() 
		setIniSetting (GetDir #plugcfg + "\\StokePreferences.ini" ) "MenuSystem" "LastVersion" CurrentVersion
	)--end if
)--end script