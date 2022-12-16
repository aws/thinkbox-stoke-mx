-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
global StokeMX_PresetDialogMode
global StokeMX_PresetObject 
local StokeMX_PresetDialog
local blueColor, redColor, greenColor, grayColor, lightGrayColor, lightBlueColor, highlightBgColor
local PresetFilterString = ""

local StokeMX_PresetIniFile = ""
local fillInTreeView, initTreeView
	
fn loadSaveButtonPressed =
(
	local st = timestamp()
	makeDir (getFilenamePath StokeMX_PresetIniFile) all:true
	if StokeMX_PresetDialogMode == #save do
	(
		deleteFile StokeMX_PresetIniFile
		local theRoot = StokeMX_PresetDialog.dn_tv.Nodes.Item[0]
		for catIndex = 1 to StokeMX_PresetTreeData[1].count do
		(
			if theRoot.Nodes.Item[catIndex-1].Checked do
			(
				for valueIndex = 1 to StokeMX_PresetTreeData[2][catIndex].count do
				(
					if theRoot.Nodes.Item[catIndex-1].Nodes.Item[valueIndex-1].Checked do
					(
						local theValue = try(getProperty (if StokeMX_PresetTreeData[2][catIndex][valueIndex][7] then StokeMX_PresetObject.delegate else StokeMX_PresetObject) StokeMX_PresetTreeData[2][catIndex][valueIndex][2])catch(undefined)
						if theValue != undefined do
							setIniSetting StokeMX_PresetIniFile StokeMX_PresetTreeData[2][catIndex][valueIndex][1] StokeMX_PresetTreeData[2][catIndex][valueIndex][2] (theValue as string)
					)--end if
				)--end valueIndex loop
			)--end if
		)--end catIndex loop
	)--end save
			
	if StokeMX_PresetDialogMode == #load do
	(
		local theRoot = StokeMX_PresetDialog.dn_tv.Nodes.Item[0]
		for catIndex = 1 to StokeMX_PresetTreeData[1].count do
		(
			if theRoot.Nodes.Item[catIndex-1].Checked do
			(
				for valueIndex = 1 to StokeMX_PresetTreeData[2][catIndex].count do
				(
					if theRoot.Nodes.Item[catIndex-1].Nodes.Item[valueIndex-1].Checked do
					(
						local theValue = getIniSetting StokeMX_PresetIniFile StokeMX_PresetTreeData[2][catIndex][valueIndex][1] StokeMX_PresetTreeData[2][catIndex][valueIndex][2]
						if StokeMX_PresetTreeData[2][catIndex][valueIndex][4] do theValue = execute theValue
						setProperty (if StokeMX_PresetTreeData[2][catIndex][valueIndex][7] then StokeMX_PresetObject.delegate else StokeMX_PresetObject) StokeMX_PresetTreeData[2][catIndex][valueIndex][2] theValue
					)--end if
				)--end valueIndex loop
			)--end if
		)--end catIndex loop			
		modPanel.setCurrentObject StokeMX_PresetObject
	)--end if load
	try(destroyDialog StokeMX_PresetDialog)catch()	
)--end fn

fn initTreeView tv =
(
	tv.nodes.Clear()
	tv.checkboxes = true
	tv.HideSelection = false
)
		
fn DateCompareFN v1 v2 valArray: =
(
	if valArray[v1] > valArray[v2] then -1 else 1
)		

fn sortFilesByDate theFiles=
(
	if theFiles.count == 0 do return #()
	
	local theYearIndex = 3 
	local theMonthIndex = 1 
	local theDayIndex = 2 
	theFileDates = for f in theFiles collect filterString (getFileCreateDate f) " /\\:.;_"
	indexArray = for i = 1 to theFileDates.count collect i
	for f = 1 to theFileDates.count do for i = 1 to theFileDates[f].count do if theFileDates[f][i].count == 1 do theFileDates[f][i] = "0" + theFileDates[f][i]
	for f = 1 to theFileDates.count where try(matchPattern theFileDates[f][7] pattern:"PM")catch(false) do theFileDates[f][4] = (execute theFileDates[f][4] + 12) as string
	theFileDates = for f in theFileDates collect f[theYearIndex]+f[theMonthIndex]+f[theDayIndex]+f[4]+f[5]+f[6]
	qsort indexArray DateCompareFN valArray:theFileDates
	for i in indexArray collect theFiles[i]
)

fn getSettingsList =
(
	#(
		--  1:INI Category				 2:Prop.Name							3:Save By Default		4:exec		5:UI Section								6: UI Name or Description			7: Is In Delegate
		#("SavingAndCaching",		"outputPath", 						false, 					false, 	"Cache Path", 							"Base Path",						false),
		#("SavingAndCaching",		"outputVersion",					false, 					false, 	"Cache Path", 							"Version",							false),
		#("SavingAndCaching",		"outputPrefix",						false, 					false, 	"Cache Path", 							"Prefix",								false),
		
		#("SavingAndCaching",		"saveResultsToDisk",				true, 					true, 	"Cache Options",	 					"Use Disk Cache",					false),
		#("SavingAndCaching",		"memoryCacheStep", 			true, 					true, 	"Cache Options",	 					"Cache Nth",						false),
		#("SavingAndCaching",		"threadLimit", 						true, 					true, 	"Cache Options",	 					"Cache Threads",					false),
		#("SavingAndCaching",		"memoryLimit", 					true, 					true, 	"Cache Options",	 					"Memory Limit",					false),
		
		#("SimulationOption",		"startTime", 						true, 					true, 	"Simulation Options",	 				"Simulate From",					false),
		#("SimulationOption",		"useCacheStartTime", 			true, 					true, 	"Simulation Options",	 				"Use Cache From",				false),
		#("SimulationOption",		"cacheStartTime", 				true, 					true, 	"Simulation Options",	 				"Cache From",						false),
		#("SimulationOption",		"endTime", 							true, 					true, 	"Simulation Options",	 				"Simulate Until",					false),
		#("SimulationOption",		"subSteps", 						true, 					true, 	"Simulation Options",	 				"Sub-Steps",						false),
		
		#("SimulationOption",		"updateViews", 					true, 					true, 	"Simulation Options",	 				"Use Preview Nth",				false),
		#("SimulationOption",		"updateViewsEvery", 			true, 					true, 	"Simulation Options",	 				"Preview Nth",						false),

		#("ParticleBirth",				"useEmitEndTime", 				true, 					true, 	"Particle Birth",	 					"Use Emit Until",					false),
		#("ParticleBirth",				"emitEndTime", 					true, 					true, 	"Particle Birth",	 					"Emit Until",							false),

		#("ParticleBirth",				"rateMode", 						true, 					true, 	"Particle Birth",	 					"Rate Mode",						false),
		#("ParticleBirth",				"rate", 								true, 					true, 	"Particle Birth",	 					"Rate/Frame",						false),

		#("ParticleLifespan",			"useLifeSpan", 					true, 					true, 	"Particle Lifespan", 					"Delete 'Dead' Particles",		false),
		#("ParticleLifespan",			"lifeSpan", 							true, 					true, 	"Particle Lifespan", 					"Particle Lifespan",				false),
		#("ParticleLifespan",			"lifeSpanVar", 						true, 					true, 	"Particle Lifespan", 					"Lifespan Variation",				false),
		
		#("Distribution",				"allowMultiPickDistribution", 	true, 					true, 	"Distribution", 							"Allow Multiple Object Picks",	false),
		#("Distribution",				"useViewportParticles", 			true, 					true, 	"Distribution", 							"Emit From Viewport Particles",false),
		#("Distribution",				"incrementRandomSeed", 		true, 					true, 	"Distribution", 							"Vary Randomness Over Time",false),
		#("Distribution",				"randomSeed", 					true, 					true, 	"Distribution", 							"Random Seed",					false),


		#("VelocityField",				"useGlobalGrid", 					true, 					true, 	"Velocity Field", 						"Use Global Grid Settings",		false),
		#("VelocityField",				"gridSize", 							true, 					true, 	"Velocity Field", 						"Global Grid Size",					false),
		#("VelocityField",				"gridPadding", 						true, 					true, 	"Velocity Field", 						"Global Grid Padding",			false),
		#("VelocityField",				"fluidMotion", 						true, 					true, 	"Velocity Field", 						"Global Create Fluid Motion",	false),

		#("Retiming",					"UsePlaybackTime", 				true, 					true, 	"Retiming", 								"Use Graph",						true),
		#("Retiming",					"UsePlaybackInterpolation", 	true, 					true, 	"Retiming", 								"Use Playback Interpolation",	true),
		

		#("ViewportDisplay",			"viewPercentage", 				true, 					true, 	"Viewport Display", 					"Percent",							false),
		#("ViewportDisplay",			"viewLimitEnabled", 				true, 					true, 	"Viewport Display", 					"Use Limit",							false),
		#("ViewportDisplay",			"viewLimit", 						true, 					true, 	"Viewport Display", 					"Limit x1000",						false),

		#("ViewportDisplay",			"ViewVectorNormalize", 			true, 					true, 	"Viewport Display", 					"Normalize Vector Length",		false),
		#("ViewportDisplay",			"viewVectorScale", 				true, 					true, 	"Viewport Display", 					"Vector Scale",					false),
		#("ViewportDisplay",			"iconSize", 							false, 					true, 	"Viewport Display", 					"Icon Size",							false)

		


	)
)

fn getSettingsListAsTree =
(
	local theSettingsList = getSettingsList()
	local theCategories = #()
	local theLeafs = #()
	for i in theSettingsList do appendIfUnique theCategories i[5]
	for i in theSettingsList do
	(
		theIndex = findItem theCategories i[5]
		if theLeafs[theIndex] == undefined do theLeafs[theIndex] = #()
		append theLeafs[theIndex] i
	)
	#(theCategories,theLeafs)
)

fn presetToLoadDDLSelected itm =
(
	if itm > 0 then
	(
		if StokeMX_PresetDialogMode == #load do 
			StokeMX_PresetIniFile = (getDir #plugcfg + "\\StokeMX\\presets\\" + StokeMX_PresetDialog.ddl_presetsToLoad.selected + ".StokeMXPreset")
		
		if doesFileExist StokeMX_PresetIniFile then
		(
			StokeMX_PresetDialog.btn_loadsave.enabled = true
		)	
		else
		(
			StokeMX_PresetDialog.btn_loadsave.enabled = false
		)	
	)	
	else	
	(
		StokeMX_PresetIniFile = ""
		StokeMX_PresetDialog.btn_loadsave.enabled = false
	)	
	initTreeView StokeMX_PresetDialog.dn_tv 
	fillInTreeView StokeMX_PresetDialog.dn_tv 			
)	
		
fn loadSaveDialogOpen =
(
	case StokeMX_PresetDialogMode of
	(
		#load: 
		(
			StokeMX_PresetDialog.title = "Stoke MX: LOAD Render Preset"
			StokeMX_PresetDialog.btn_loadsave.caption = "LOAD PRESET..." 
			StokeMX_PresetDialog.lbl_edittext.text = "Filter Pattern:"
			theFiles = getFiles (getDir #plugcfg + "\\StokeMX\\presets\\"+ PresetFilterString +"*.StokeMXPreset")
			if theFiles.count == 0 then 
			(
				StokeMX_PresetDialog.btn_loadsave.enabled = false
				StokeMX_PresetDialog.ddl_presetsToLoad.items = #()
			)	
			else
			(
				theFiles = sortFilesByDate theFiles 
				StokeMX_PresetDialog.ddl_presetsToLoad.items = for f in theFiles collect getFileNameFile f
				StokeMX_PresetDialog.ddl_presetsToLoad.selection = 1
				presetToLoadDDLSelected StokeMX_PresetDialog.ddl_presetsToLoad.selection
				StokeMX_PresetIniFile = (getDir #plugcfg + "\\StokeMX\\presets\\" + StokeMX_PresetDialog.ddl_presetsToLoad.selected + ".StokeMXPreset")
			)	
		)
		#save: 
		(
			theFiles = getFiles (getDir #plugcfg + "\\StokeMX\\presets\\*.StokeMXPreset")
			theFiles = sortFilesByDate theFiles 
			StokeMX_PresetDialog.ddl_presetsToLoad.items = for f in theFiles collect getFileNameFile f
		
			StokeMX_PresetDialog.title = "Stoke MX: SAVE Render Preset"
			StokeMX_PresetDialog.btn_loadsave.caption = "SAVE PRESET..." 
			StokeMX_PresetDialog.lbl_edittext.text = "Preset Name:"
			local thePresetFileName = if maxFileName != "" then (getFileNameFile maxFileName) else "Preset"
			StokeMX_PresetDialog.edt_presetsToSave.text = thePresetFileName
			StokeMX_PresetIniFile = (getDir #plugcfg + "\\StokeMX\\presets\\" + StokeMX_PresetDialog.edt_presetsToSave.text + ".StokeMXPreset")
		)
	)		
)	

fn fillInTreeView tv =
(
	local st = timestamp()
	theRoot = tv.nodes.add "" 
	
	local textColor = ( ((colorman.getcolor #text) as color)*255)
	local maxBgColor = (((colorman.getcolor #window)) as color)*255
	tv.foreColor = (dotNetClass "System.Drawing.Color").fromARGB textColor.r textColor.g textColor.b
	tv.backColor = (dotNetClass "System.Drawing.Color").fromARGB maxBgColor.r maxBgColor.g maxBgColor.b
	
	local blackColor = (dotNetClass "System.Drawing.Color").fromARGB textColor.r textColor.g textColor.b
	if maxBgColor.v >= 160 then
	(
		redColor = (dotNetClass "System.Drawing.Color").fromARGB 192 0 0
		greenColor = (dotNetClass "System.Drawing.Color").fromARGB 0 128 0
		blueColor = (dotNetClass "System.Drawing.Color").fromARGB 0 0 128
		lightBlueColor = (dotNetClass "System.Drawing.Color").fromARGB 90 128 160
		grayColor = (dotNetClass "System.Drawing.Color").fromARGB 128 128 128
		lightGrayColor = (dotNetClass "System.Drawing.Color").fromARGB 160 160 160
		highlightBgColor = (dotNetClass "System.Drawing.Color").fromARGB 180 190 200
	)
	else
	(
		redColor = (dotNetClass "System.Drawing.Color").fromARGB 255 128 128
		greenColor = (dotNetClass "System.Drawing.Color").fromARGB 128 255 128
		blueColor = (dotNetClass "System.Drawing.Color").fromARGB 170 200 255
		lightBlueColor = (dotNetClass "System.Drawing.Color").fromARGB 128 160 255
		grayColor = (dotNetClass "System.Drawing.Color").fromARGB 192 192 192
		lightGrayColor = (dotNetClass "System.Drawing.Color").fromARGB 160 160 160
		highlightBgColor = (dotNetClass "System.Drawing.Color").fromARGB 20 40 60
	)			
	
	case StokeMX_PresetDialogMode of
	(
		#save: 
		(
			StokeMX_PresetDialog.title = "Stoke MX: SAVE Preset"
			theRoot.text = "SAVE Preset"
			theRoot.forecolor = redColor
		)	
		#load: 
		(
			StokeMX_PresetDialog.title = "Stoke MX: LOAD Preset"
			if StokeMX_PresetDialog.ddl_presetsToLoad.selected != undefined then 
				theRoot.text = "LOAD Preset [" + StokeMX_PresetDialog.ddl_presetsToLoad.selected + "]"
			else	
				theRoot.text = "NO PRESETS FOUND!"
			theRoot.forecolor = greenColor
		)	
	)	
	theRoot.checked = true
	
	::StokeMX_PresetTreeData = getSettingsListAsTree()
	if StokeMX_PresetDialogMode == #load do
	(
		for catIndex = 1 to StokeMX_PresetTreeData[1].count do
		(				
			for valueIndex = StokeMX_PresetTreeData[2][catIndex].count to 1 by -1 do
			(
				theKeys = getIniSetting StokeMX_PresetIniFile StokeMX_PresetTreeData[2][catIndex][valueIndex][1] 
				if findItem theKeys StokeMX_PresetTreeData[2][catIndex][valueIndex][2] == 0 do deleteItem StokeMX_PresetTreeData[2][catIndex] valueIndex
			)
		)
		for catIndex = StokeMX_PresetTreeData[1].count to 1 by -1 do
		(
			if StokeMX_PresetTreeData[2][catIndex].count == 0 do 
			(
				deleteItem StokeMX_PresetTreeData[1] catIndex
				deleteItem StokeMX_PresetTreeData[2] catIndex
			)
		)
	)
	
	for catIndex = 1 to StokeMX_PresetTreeData[1].count do
	(
		newNode = theRoot.Nodes.add StokeMX_PresetTreeData[1][catIndex]
		newNode.checked = false
		newNode.forecolor = blueColor
		for valueIndex = 1 to StokeMX_PresetTreeData[2][catIndex].count do
		(
			if StokeMX_PresetDialogMode == #save then
			(
				newSubNode = newNode.Nodes.add (StokeMX_PresetTreeData[2][catIndex][valueIndex][6]+" ["+ (try((getProperty (if StokeMX_PresetTreeData[2][catIndex][valueIndex][7] then StokeMX_PresetObject.delegate else StokeMX_PresetObject) StokeMX_PresetTreeData[2][catIndex][valueIndex][2]) as string )catch("--")) +"]")
				newSubNode.checked = StokeMX_PresetTreeData[2][catIndex][valueIndex][3]
				newNode.checked = true
				newSubNode.forecolor = if newSubNode.checked then redColor else grayColor
			)
			else
			(
				local fileValue = getIniSetting StokeMX_PresetIniFile StokeMX_PresetTreeData[2][catIndex][valueIndex][1] StokeMX_PresetTreeData[2][catIndex][valueIndex][2]
				local structValue = (try((getProperty (if StokeMX_PresetTreeData[2][catIndex][valueIndex][7] then StokeMX_PresetObject.delegate else StokeMX_PresetObject) StokeMX_PresetTreeData[2][catIndex][valueIndex][2]) as string )catch("--"))
				local textToDisplay = StokeMX_PresetTreeData[2][catIndex][valueIndex][6] + " ["+ structValue +"]"
				newSubNode = newNode.Nodes.add ""
				if fileValue != structValue then
				(
					textToDisplay+=" -> ["+ fileValue +"]"
					newSubNode.checked = true
					newNode.checked = true
					newSubNode.forecolor = greenColor
					newNode.expand()
				)
				else
				(
					newSubNode.checked = false
					newSubNode.forecolor = grayColor
				)
				newSubNode.text = textToDisplay
			)
		)--end valueIndex loop
	)
	theRoot.expand()
)

fn displayPresetDialog =
(
	local st = timestamp()
	try(destroyDialog StokeMX_PresetDialog)catch()
	rollout StokeMX_PresetDialog "" 
	(
		button btn_resetAll "Reset" align:#left width:45 height:20 pos:[5,1]  tooltip:"Reset all Categories and Properties to their start-up checked states."
		button btn_allCatOn "Check All Categories" align:#center width:110 height:20 pos:[52,1] tooltip:"Checks all Categories.\nEquivalent to checking the Root checkbox.\n\nThis will NOT affect the Properties in each Category, only the top-level checkboxes, but this will skip the complete Category!\n\nTo UNCHECK all Categories, simply press [Check All Categories] and then [Invert Categories]!"
		button btn_allCatinvert "Invert Categories" align:#center width:100 height:20 pos:[164,1] tooltip:"Inverts the checked state of all Categories. \n\nThis will NOT affect the Properties in each Category, only the top-level checkboxes."
		button btn_allPropsOn "Check Properties" align:#center width:90 height:20 pos:[266,1] tooltip:"Select a Category, then press to check all Properties that belong to it, as well as the Category itself."
		button btn_allPropsInvert "Invert Properties" align:#right width:90 height:20 pos:[358,1] tooltip:"Select a Category, then press to invert the checked state of all Properties that belong to it."
		edittext edt_searchField pos:[447,2] fieldwidth:143
		
		dotNetControl dn_tv "TreeView" width:590 height:590 align:#center offset:[0,-3]
		
		button btn_cancel "Cancel" height:42 width:60 align:#left across:2 offset:[-5,-1]
		button btn_loadsave "SAVE PRESET..." height:42 width:100 align:#right offset:[5,-1]
		label lbl_edittext "Filter Pattern:" pos:[75,618]
		edittext edt_presetsToSave text:"" width:343 pos:[140,617]
		dropdownlist ddl_presetsToLoad width:410 pos:[75,636]
		
		
		on btn_resetAll pressed do
		(
			initTreeView dn_tv
			fillInTreeView dn_tv
		)
		on btn_allCatOn pressed do
		(
			dn_tv.Nodes.Item[0].checked = true
			for i = 1 to dn_tv.Nodes.Item[0].Nodes.count do
			(
				dn_tv.Nodes.Item[0].Nodes.Item[i-1].Checked = true
			)
		)
		on btn_allCatinvert pressed do
		(
			for i = 1 to dn_tv.Nodes.Item[0].Nodes.count do
			(
				dn_tv.Nodes.Item[0].Nodes.Item[i-1].Checked = not dn_tv.Nodes.Item[0].Nodes.Item[i-1].Checked
			)
		)		
		on btn_allPropsOn pressed do
		(
			for i = 1 to dn_tv.Nodes.Item[0].Nodes.count where dn_tv.Nodes.Item[0].Nodes.Item[i-1].IsSelected == true do
			(
				dn_tv.Nodes.Item[0].Nodes.Item[i-1].checked = true
				for j = 1 to dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.count do
					dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].Checked = true
			)					
		)
		on btn_allPropsInvert pressed do
		(
			for i = 1 to dn_tv.Nodes.Item[0].Nodes.count where dn_tv.Nodes.Item[0].Nodes.Item[i-1].IsSelected == true do
			(
				for j = 1 to dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.count do
					dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].Checked = not dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].Checked
			)					
		)	
			
		on btn_cancel pressed do 
		(
			try(destroyDialog StokeMX_PresetDialog)catch()
		)

		on btn_loadsave pressed do 
		(
			if edt_presetsToSave.text != "" or StokeMX_PresetDialogMode == #load do
				loadSaveButtonPressed()
		)

		on dn_tv AfterCheck arg do 
		(
			if arg.Node.parent == undefined then
			(
				for i = 1 to dn_tv.Nodes.Item[0].Nodes.count do
					dn_tv.Nodes.Item[0].Nodes.Item[i-1].Checked = arg.Node.Checked
			)
			else 
			(
				if arg.Node.parent != dn_tv.Nodes.Item[0] do
				(
					local theParent = arg.Node.parent
					local isChecked = 0
					for i = 1 to theParent.Nodes.count do
					(
						if theParent.Nodes.Item[i-1].Checked then
						(
							if StokeMX_PresetDialogMode == #save do theParent.Nodes.Item[i-1].forecolor = redColor
							isChecked+=1
						)
						else
							if StokeMX_PresetDialogMode == #save do theParent.Nodes.Item[i-1].forecolor = grayColor
					)
					theParent.forecolor = case of
					(
						(isChecked == 0): lightGrayColor
						default: lightBlueColor
						(isChecked == theParent.Nodes.count): blueColor
					)
				)
			)
		)
		
		on edt_searchField changed txt do
		(
			for i = 1 to dn_tv.Nodes.Item[0].Nodes.count do
			(
				local expandedNode = false
				for j = 1 to dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.count do
				(
					if txt != "" and matchPattern dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].Text pattern:("*"+txt+"*") then
					(
						dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].backcolor = highlightBgColor
						expandedNode = true
					)
					else
						dn_tv.Nodes.Item[0].Nodes.Item[i-1].Nodes.Item[j-1].backcolor = bgColor
				)
				if expandedNode then 
				(
					dn_tv.Nodes.Item[0].Nodes.Item[i-1].expand() 
				)
				else 
				(
					dn_tv.Nodes.Item[0].Nodes.Item[i-1].collapse()
				)
			)						
		)

		on edt_presetsToSave entered txt do 
		(
			if StokeMX_PresetDialogMode == #save do 
			(
				StokeMX_PresetIniFile = ( getDir #plugcfg + "\\StokeMX\\presets\\" + txt + ".StokeMXPreset") 
			)
		)
		
		on edt_presetsToSave changed txt do 
		(
			if StokeMX_PresetDialogMode != #save then
			(
				PresetFilterString = txt
				loadSaveDialogOpen()
				presetToLoadDDLSelected ddl_presetsToLoad.selection
			)
			else btn_loadsave.enabled = txt.count > 0
		)		
		
		on ddl_presetsToLoad selected itm do 
		(
			if StokeMX_PresetDialogMode == #save then
				edt_presetsToSave.entered (edt_presetsToSave.text = ddl_presetsToLoad.selected)
			else
				presetToLoadDDLSelected itm
		)	
			
		on StokeMX_PresetDialog open do 
		(
			if StokeMX_PresetDialogMode != #save do edt_presetsToSave.text = PresetFilterString 
			loadSaveDialogOpen()
			initTreeView dn_tv
			fillInTreeView dn_tv
		)
	)
	createDialog StokeMX_PresetDialog 600 660 modal:true
)	

displayPresetDialog()

)