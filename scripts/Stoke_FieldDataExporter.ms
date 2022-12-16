-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript StokeFieldDataExporter category:"Stoke" buttontext:"Export" tooltip:"Open the Field Data Exporter Dialog" icon:#("Stoke",10)
(
global Stoke_Field_Data_Exporter_Dialog 
try(destroyDialog Stoke_Field_Data_Exporter_Dialog )catch()
local theIniFile = (getDir #plugcfg + "//StokeFieldDataExporterPreferences.ini")
rollout Stoke_Field_Data_Exporter_Dialog "Stoke Field Data Exporter"
(
	local theStokeFieldObjects = #()
	
	button btn_refreshList "Refresh" align:#right height:20 offset:[2,-3]
	dropdownlist ddl_fields "Stoke Field scene object to Export:" align:#center width:280 offset:[0,-22] 
	group "Save Frame Range:"
	(
		spinner spn_startFrame "From Frame:" type:#integer range:[-1000000,1000000,animationrange.start.frame] fieldwidth:50 across:2 
		spinner spn_endFrame "To Frame:" type:#integer range:[-1000000,1000000,animationrange.end.frame] fieldwidth:50
		button btn_sceneRange "Scene Range" width:90 across:3
		button btn_renderRange "Render Range" width:90 
		button btn_currentFrame "Current Frame" width:90 
	)
	
	group "Save Path"
	(
		button btn_basePath "Base" across:2 width:35 align:#left height:17 offset:[-5,-3]
		edittext edt_basePath "" text:"$default" width:251 align:#right offset:[5,-3]

		button btn_ProjectName "Proj." width:35  across:4 align:#left height:17 offset:[-5,-1]
		edittext edt_ProjectName "" width:160 align:#center offset:[6,-1]
		button btn_Version "Ver." width:35  align:#left height:17 offset:[55,-1]
		edittext edt_Version "" text:"v001" fieldwidth:50 align:#right offset:[7,-1]
		
		button btn_FileName "File" across:3 width:35  align:#left  height:17 offset:[-5,-1]
		edittext edt_FileName "" text:"$object" width:200 align:#center offset:[-8,-1]
		dropdownlist ddl_exportFormat items:#(".FXD",".F3D",".VDB") width:52 align:#right offset:[7,-3] 
	)
	checkbox chk_useRenderState "Switch All Scene Objects To Render State" offset:[0,-3]
	--progressbar prg_progress height:6 color:blue align:#center width:280
	checkbutton chk_exportwarning "The .FXD files will work only in FumeFX v5.0 or higher!" height:20 align:#center width:292 border:false checked:true highlightcolor:red 
	on chk_exportwarning changed state do chk_exportwarning.checked = true
	button btn_export "INITIALIZING STOKE FIELD DATA EXPORTER..." height:35 align:#center width:292 offset:[0,-3]
	
	fn getZeros theNumber =
	(
		theCount = (theNumber as string).count
		if theCount < 999 then
			substring "000" 1 (3-theCount)
		else
			""	
	)	

	fn getBaseFileName =
	(
		local txt = edt_fileName.text
		if matchPattern txt pattern:"*$object*" do 
		(
			if ddl_fields.selected != undefined then
				txt = substituteString (toLower txt) "$object" (ddl_fields.selected+"_")
			else
				txt = "untitled_"
		)
		while matchPattern txt pattern:"*$*" do
		(
			txt = substituteString txt "$" "_"
		)
		txt
	)	
	
	fn getCachePath =
	(
		local outputPath = edt_basePath.text
		local theString = outputPath
		if outputPath == "" do outputPath = "$default\\"
		local theDefaultPath = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\Export\\"
		while findString theString "$scene" != undefined do
			theString = substituteString (toLower theString) "$scene" (getFileNameFile maxFileName)
		while findString theString "$user" != undefined do
			theString = substituteString (toLower theString) "$user" sysinfo.userName
		if matchPattern theString pattern:"$default\\*" do
			theString = substituteString (toLower theString) "$default\\" theDefaultPath
		if matchPattern theString pattern:"$temp*" do
			theString = substituteString (toLower theString) "$temp\\" ((getDir #temp)+"\\")
		for i = 1 to theString.count do if findString "$*!\"" theString[i] != undefined do theString[i] = "_"
		--format "%\n" theString
		if not matchPattern theString pattern:"*\\" do theString+= "\\"
		theString
	)
	
	fn getLatestVersionFolder =
	(
		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + edt_projectName.text+"\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)		
		edt_version.text = if theDirNames.count > 0 then theDirNames[theDirNames.count] else "v001"
	)		
	
	fn updatePaths =
	(
		local theVal = getIniSetting theIniFile "Path" "Base"
		if theVal == "" do theVal = "$default\\"
		edt_basePath.text = theVal
		
		local theVal = getIniSetting theIniFile "Path" "Project"
		if theVal == "" do theVal = "StokeProject"
		edt_ProjectName.text = theVal
		
		getLatestVersionFolder()
		
		/*if ddl_fields.selection > 0 do 
		edt_FileName.text = ddl_fields.selected */
	)	
	
	fn createNewVersion theVersion =
	(
		local theCachePath = getCachePath()
		makedir (theCachePath+edt_projectName.text+"\\"+theVersion) all:true		
		edt_Version.text = theVersion
	)
	
	fn getNewVersionFolder =
	(
		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + edt_projectName.text +"\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		local maxNumber = 0
		for d in theDirNames do
		(
			local theNumber = ""
			for i = d.count to 1 by -1 do 
				if findstring "0123456789" d[i] != undefined then theNumber = d[i]+theNumber else exit
			theNumber = theNumber as integer		
			if theNumber > maxNumber do maxNumber = theNumber		
		)
		"v" + (getZeros (maxNumber+1)) + (maxNumber+1) as string		
	)
	
	fn updateUI =
	(
		theStokeFieldObjects = for o in objects where StokeGlobalInterface.IsEmberField o collect o
--			findItem #(StokeFieldSIM, Stoke_Field) (classof o.baseobject) > 0 collect o
		ddl_fields.items = for o in theStokeFieldObjects collect o.name
		if selection.count == 1 and (theIndex = (findItem theStokeFieldObjects selection[1])) > 0 do ddl_fields.selection = theIndex
		btn_export.enabled = theStokeFieldObjects.count > 0 --and ddl_exportFormat.selection < 3
	
		local txt = "EXPORT Stoke Field Data To " 
		txt += (case ddl_exportFormat.selection of
		(
			1: "FumeFX .FXD "
			2: "Field3D .F3D "
			3: "OpenVDB .VDB "
		) 
		)
		txt += (if spn_endFrame.value-spn_startFrame.value > 0 then "Files..." else "File...")
		btn_export.text = txt
		chk_exportwarning.visible = ddl_exportFormat.selection == 1
		updatePaths()
	)	
	
	fn createProjectRCMenu =
	(
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"

		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + "\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		
		for f in theDirNames do
		(
			txt += "menuItem mnu_"+f+" \""+ f + "\" \n"
			txt += "on mnu_"+f+" picked do (Stoke_Field_Data_Exporter_Dialog.edt_projectName.text = \""+ f +"\" )\n"
		)
		
		txt += "separator sep_10\n"
		txt += "menuItem mnu_exploreCurrentFolder \"EXPLORE Current Project Folder ["+edt_projectName.text+"]...\" \n"
		txt += "on mnu_exploreCurrentFolder picked do (shellLaunch \""+ theCachePath + "\\" + edt_projectName.text +"\" \"\")\n"
		
			
		txt += ")\n"
		execute txt
	)		
	
	fn createVersionRCMenu =
	(
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"

		local nextVersion = getNewVersionFolder()
			
		txt += "menuItem mnu_NextVersion \"NEW Version ["+nextVersion+"]\" \n"
		txt += "on mnu_NextVersion picked do (Stoke_Field_Data_Exporter_Dialog.createNewVersion \""+ nextVersion +"\" )\n"
		txt += "separator sep_10\n"

		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + edt_projectName.text+"\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		
		for f in theDirNames do
		(
			txt += "menuItem mnu_"+f+" \""+ f + "\" \n"
			txt += "on mnu_"+f+" picked do (Stoke_Field_Data_Exporter_Dialog.createNewVersion \""+ f +"\" )\n"
		)
		
		txt += "separator sep_20\n"
		txt += "menuItem mnu_exploreCurrentFolder \"EXPLORE Current Version Folder ["+edt_Version.text+"]...\" \n"
		txt += "on mnu_exploreCurrentFolder picked do (shellLaunch \""+ theCachePath + "\\" + edt_projectName.text + "\\" + edt_Version.text +"\" \"\")\n"
		
			
		txt += ")\n"
		execute txt
	)	
	
	fn createBaseRCMenu =
	(
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"
		txt += "menuItem mnu_Reset \"RESET to $default\" \n"
		txt += "on mnu_Reset picked do (::Stoke_Field_Data_Exporter_Dialog.resetBasePath()  )\n"
		txt += "separator sep_10\n"
		txt += "menuItem mnu_Pick \"PICK A New Base Path...\" \n"
		txt += "on mnu_Pick picked do (::Stoke_Field_Data_Exporter_Dialog.pickBasePath() )\n"
		
		local theCachePath = getCachePath()
		if doesFileExist theCachePath do
		(
			txt += "separator sep_20\n"
			txt += "menuItem mnu_ExploreFolder \"EXPLORE Base Output Folder...\" \n"
			txt += "on mnu_ExploreFolder picked do (shellLaunch @\""+theCachePath+"\" \"\") \n"
		)
		txt += ")\n"
		execute txt		
	)	
	
	fn pickBasePath =
	(
		local outputPath = ""
		theNewPath = getSavePath initialDir:(getCachePath())
		if theNewPath != undefined do outputPath = theNewPath
		if outputPath == "" do outputPath = "$default\\"
		setIniSetting theIniFile "Path"	"Base" outputPath
		updatePaths()		
	)
	
	fn resetBasePath =
	(
		setIniSetting theIniFile "Path"	"Base" "$default\\"
		updatePaths()	
	)

	on edt_ProjectName entered txt do
	(
		setIniSetting theIniFile "Path"	"Project" txt
		updatePaths()	
	)

	
	fn createFileRCMenu =
	(
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"
		txt += "menuItem mnu_Reset \"RESET to $object\" \n"
		txt += "on mnu_Reset picked do (::Stoke_Field_Data_Exporter_Dialog.edt_filename.text = \"$object\" )\n"
		
		local theCachePath = getCachePath()
		local theFile = theCachePath+edt_projectName.text+"\\"+edt_version.text+"\\" 
		makedir theFile all:true	
		if doesFileExist theFile do
		(
			txt += "separator sep_10\n"
			
			txt += "menuItem mnu_CopyPathToClipboard \"COPY PATH To Windows Clipboard...\" \n"
			txt += "on mnu_CopyPathToClipboard picked do (setclipboardText @\""+theFile+"\") \n"
			
			txt += "menuItem mnu_CopyFilenameToClipboard \"COPY FILENAME To Windows Clipboard...\" \n"
			txt += "on mnu_CopyFilenameToClipboard picked do (setclipboardText @\""+theFile+getBaseFileName()+(formattedPrint (currentTime.frame as integer) format:"04i")+ddl_exportFormat.selected+"\") \n"

			txt += "separator sep_20\n"
			txt += "menuItem mnu_ExploreFolder \"EXPLORE Output Folder...\" \n"
			txt += "on mnu_ExploreFolder picked do (shellLaunch @\""+theFile+"\" \"\") \n"
		)
		txt += ")\n"
		execute txt		
	)
	
	on btn_projectName pressed do
	(
		createProjectRCMenu()
		popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
	)
	
	on btn_Version pressed do 
	(
		createVersionRCMenu()
		popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
	)
	
	on btn_FileName pressed do
	(
		createFileRCMenu()
		popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
	)	
	
	on edt_basePath entered txt do
	(
		if matchPattern txt pattern:"$default*" and not matchPattern txt pattern:"$default\\*" do 
			txt = substituteString (toLower txt) "$default" "$default\\"

		if matchPattern txt pattern:"$temp*" and not matchPattern txt pattern:"$temp\\*" do 
			txt = substituteString (toLower txt) "$temp" "$temp\\"
		
		if matchPattern txt pattern:"*$default*" and not matchPattern txt pattern:"$default*" do 
			txt = substituteString (toLower txt) "$default" ""

		if matchPattern txt pattern:"*$temp*" and not matchPattern txt pattern:"$temp*" do 
			txt = substituteString (toLower txt) "$temp" ""
		
		for i = 1 to txt.count do if findString "?*" txt[i] != undefined do txt[i] = "_"
		outputPath = if txt == "" then "$default\\" else txt
			
		if not matchPattern outputPath pattern:"*\\" do outputPath += "\\"

		--format "Creating: %\n" (getCachePath())
		makeDir (getCachePath()) all:true
		if not doesFileExist (getCachePath()) do outputPath  = "$default\\"
		updatePaths()
	)	
	
	on btn_basePath pressed do
	(
		createBaseRCMenu()
		popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
	)
	
	on btn_refreshList pressed do updateUI()
	
	on ddl_exportFormat selected itm do
	(
		updateUI()
	)
	
	on edt_version entered txt do
	(
		if txt != "" do
			createNewVersion txt
		updateUI()
	)
	
	on btn_sceneRange pressed do
	(
		spn_startFrame.value = animationrange.start.frame as integer
		spn_endFrame.value = animationrange.end.frame as integer
		updateUI()
	)
	on btn_renderRange pressed do
	(
		spn_startFrame.value = rendStart
		spn_endFrame.value = rendEnd
		updateUI()
	)	
	on btn_currentFrame pressed do 
	(
		spn_startFrame.value = spn_endFrame.value = sliderTime.frame as integer
		updateUI()
	)
	
	on spn_startFrame changed val do 
	(
		if val > spn_endFrame.value do spn_endFrame.value = val
		updateUI()		
	)
	on spn_endFrame changed val do 	
	(
		if val < spn_startFrame.value do spn_startFrame.value = val
		updateUI()
	)
	
	on btn_export pressed do
	(
		if theStokeFieldObjects.count > 0 do
		(
			local theField = theStokeFieldObjects[ddl_fields.selection]
			local theCachePath = getCachePath()
			makedir (theCachePath+edt_projectName.text+"\\"+edt_version.text) all:true	
			local	theBaseFilename = getBaseFileName()
			local theFile = theCachePath+edt_projectName.text+"\\"+edt_version.text+"\\" + theBaseFilename 
			format "--Exporting to Path [%%]\n" theFile ddl_exportFormat.selected
			/*
			local typesString = case ddl_exportFormat.selection of
			(
				1: "FumeFX Simulation (*.fxd)|*.fxd"
				2: "Field3D Sequence (*.f3d)|*.f3d"
				3: "OpenVDB Sequence (*.vdb)|*.vdb"
			)
			local theFile = getSaveFilename filename:((substituteString theField.name " " "_") +"_") types:typesString	
			*/
			--if theFile != undefined do
			(
				local theFileNameFile = getFileNameFile theFile
				if try(isKindof (execute (substring theFileNameFile (theFileNameFile.count-3) 4)) Number)catch(false) do 
					theFileNameFile = substring theFileNameFile 1 (theFileNameFile.count-4)
				local stopped = false

				local allObjects = objects as array
				if chk_useRenderState.state do StokeGlobalInterface.BeginRenderMode allObjects
				sliderTime = spn_startFrame.value + 1
				max views redraw
				if ddl_exportFormat.selection == 1 do
				(
					sliderTime = spn_startFrame.value
					local f = StokeGlobalInterface.CreateMXSField theField
					local theScriptFileName = (getFileNamePath theFile + "CREATE_FUMEFX_" +theFileNameFile +".ms" ) 
					local theScriptFile = createFile theScriptFileName
					if theScriptFile != undefined do
					(
						format "(theFume = FumeFX()\n" to:theScriptFile 
						format "theFume.pos = [%,%,%]\n" (f.BoundsMin.x+(f.BoundsMax.x - f.BoundsMin.x)/2) (f.BoundsMin.y+(f.BoundsMax.y - f.BoundsMin.y)/2)  (f.BoundsMin.z) to:theScriptFile 
						format "theFume.gridspacing = %\n" f.Spacing to:theScriptFile 
						format "theFume.width = %\n" (f.BoundsMax.x - f.BoundsMin.x) to:theScriptFile 
						format "theFume.length = %\n" (f.BoundsMax.y - f.BoundsMin.y) to:theScriptFile 
						format "theFume.height = %\n" (f.BoundsMax.z - f.BoundsMin.z) to:theScriptFile 
						format "max modify mode \n" to:theScriptFile 
						format "select theFume \n" to:theScriptFile 
						format "try(theFume.SetPath @\"%\" \"default\")catch()\n" (getFileNamePath theFile + theFileNameFile +(formattedPrint spn_startFrame.value format:"04i")+".fxd" )  to:theScriptFile 
						format ")\n" to:theScriptFile 
						close theScriptFile 
					)
					f.Clear()
				)
				progressStart ("Exporting to "+ddl_exportFormat.selected)
				for t = spn_startFrame.value to spn_endFrame.value while not stopped do 
				(
					try
					(
						stopped = not (progressUpdate (100.0 * (t-spn_startFrame.value) / (spn_endFrame.value-spn_startFrame.value)))
						sliderTime = t
						local f = StokeGlobalInterface.CreateMXSField theField
						--showInterface f
						case ddl_exportFormat.selection of
						(
							1: StokeGlobalInterface.WriteFXDFile (getFileNamePath theFile + theFileNameFile +(formattedPrint (currentTime.frame as integer) format:"04i")+".fxd" )  theField  f.BoundsMin f.BoundsMax f.Spacing
							2: StokeGlobalInterface.WriteField3DFile (getFileNamePath theFile + theFileNameFile +(formattedPrint (currentTime.frame as integer) format:"04i")+".f3d" ) theField  f.BoundsMin f.BoundsMax f.Spacing
							3: StokeGlobalInterface.WriteOpenVDBFile (getFileNamePath theFile + theFileNameFile +(formattedPrint (currentTime.frame as integer) format:"04i")+".vdb" ) theField  f.BoundsMin f.BoundsMax f.Spacing
						)
						f.Clear()
					)
					catch
					(
						stopped = true
						local txt = "" as stringstream
						format "Field Export FAILED on Frame % due to an ERROR:\n" t to:txt
						format "%\n" (getCurrentException()) to:txt
						format "\nPlease ensure all channels in the source Field object are valid!\n" to:txt
						messagebox (txt as string) title:"Error During Field Export"
					)
				)--end t loop
				--prg_progress.value = 0
				if chk_useRenderState.state do StokeGlobalInterface.EndRenderMode allObjects
				progressEnd()
			)
		)
	)
	
	on Stoke_Field_Data_Exporter_Dialog moved pos do
	(
		setIniSetting theIniFile "Dialog" "Position" (pos as string)
	)
	
	on Stoke_Field_Data_Exporter_Dialog open do
	(
		updateUI()
		getLatestVersionFolder()

	)
)
local theVal = execute (getIniSetting theIniFile "Dialog" "Position")
if theVal == OK then 
	createDialog Stoke_Field_Data_Exporter_Dialog 300 290
else
	createDialog Stoke_Field_Data_Exporter_Dialog 300 290 theVal.x theVal.y

)