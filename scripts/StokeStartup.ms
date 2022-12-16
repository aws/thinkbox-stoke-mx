-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
local theSRoot = try(StokeGlobalInterface.HomeDirectory)catch(undefined)
if theSRoot != undefined then
(
	if not (IsNetServer()) do --if running in workstation mode,
	(
		Stoke_PresetsArrowBitmap_Base = openbitmap (theSRoot + "\\icons\\StokeUI.bmp")
		::Stoke_PresetsArrowBitmap = bitmap 512 16 color:red 
		copy Stoke_PresetsArrowBitmap_Base Stoke_PresetsArrowBitmap 
		
		local iconFiles = #( "Stoke_24i.bmp","Stoke_24a.bmp","Stoke_16i.bmp","Stoke_16a.bmp","Genome_24i.bmp", "Genome_24a.bmp", "Genome_16i.bmp", "Genome_16a.bmp")
		local updatedIcons = 0 --count the updates
		for i in iconFiles do --go through all icon files
		(
			local theTarget = (GetDir #usericons + "\\" + i) --this is the target name in the user Icons folder (this is necessary for Vista!)
			local theSource = (theSRoot + "Icons\\" + i) --this is the source file in Stoke's Icons folder
			--if the target file exists and is different or does not exist at all, 
			if (doesFileExist theSource and doesFileExist theTarget and getFileModDate theTarget != getFileModDate theSource) or (not doesFileExist theTarget) then
			(
				deleteFile theTarget  --delete the old file (MAXScript does not support direct overwriting)
				local result = copyFile theSource theTarget  --copy the new file to the Max Icons folder
				if result do updatedIcons += 1 --increment the counter.
			)	
		)	
		if updatedIcons > 0 do --if any icons were copied over,
		(
			colorMan.reiniticons() --reinitialize the icons
		)
		
		local theBLOPFolder = theSRoot + "\\BlackOps\\"
		
		--make sure the shared folder exists
		local theKTargetFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\Krakatoa\\MagmaFlowSettings\\"
		makeDir theKTargetFolder all:true
		
		for aProduct in #("StokeField","StokeParticles","Genome") do
		(
			local theTargetFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\"+aProduct+"\\BlackOps\\"
			local theDirs = getDirectories (theBLOPFolder +"*")
			for d in theDirs do
			(
				local theFS = filterString d "\\/"
				local theFolder = theFS[theFS.count]
				makeDir (theTargetFolder+theFolder) all:true
				for f in getFiles (d+"*.MagmaBLOP") do 
				(
					local result = copyFile f (theTargetFolder+theFolder+"\\"+filenameFromPath f)
				)--end f loop
			)--end d loop
		)--end aProduct loop

		local thePresetsFolder = theSRoot + "\\Presets\\"
		local theTargetFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\MagmaFlows\\"
		local theDirs = getDirectories (thePresetsFolder +"*")
		for d in theDirs do
		(
			local theFS = filterString d "\\/"
			local theFolder = theFS[theFS.count]
			makeDir (theTargetFolder+theFolder) all:true
			for f in getFiles (d+"*.MagmaScript") do 
			(
				local result = copyFile f (theTargetFolder+theFolder+"\\"+filenameFromPath f)
			)--end f loop
		)--end d loop
		
		
	)--end if workstation mode
	
	global Stoke_autoLayoutOnResizeOn = true											--we start with a value of True
	local maxVer = maxVersion()															--we get an array of version values - unfortunately, the return value varies between 3 elements in 2018 and earlier, and 9 elements in 2019+
	if maxVer[1] == 22000 and maxVer[5] > 0 do ::Stoke_autoLayoutOnResizeOn = false		--if the first element is 22000 (corresponding to v22, a.k.a. 3dsMax 2020), then there is a valid 5th element. If it is 1 or higher, set to False to avoid an Autodesk bug
	::Stoke_autoLayoutOnResizeOn = false												--overriding to False for all 3ds Max versions for release, as auto-layout is currently deemed unreliable. Comment out this line to re-enable in 2019 and 2020.0.
	
	--Set Current Directory to the Scripts folder in Stoke Home:
	sysinfo.currentDir = theSRoot + "Scripts"
	fileIn (theSRoot + "Scripts\\StokeUI.ms")
	fileIn (theSRoot + "Scripts\\StokePartitioning.ms")
	
	fileIn (theSRoot + "Scripts\\Magma_ImplementationObjects.ms")
	fileIn (theSRoot + "Scripts\\StokeField_MagmaFlowManager.ms")
	fileIn (theSRoot + "Scripts\\Stoke_Field_Creator.ms")
	fileIn (theSRoot + "Scripts\\Stoke_Field.ms")
	fileIn (theSRoot + "Scripts\\StokeField_SimUI.ms")
	fileIn (theSRoot + "Scripts\\Stoke_FieldProbe.ms")
	fileIn (theSRoot + "Scripts\\StokeField_GridManipulator.ms")
	fileIn (theSRoot + "Scripts\\Stoke_FieldDataExporter.ms")
	fileIn (theSRoot + "Scripts\\Stoke2FGA.ms")
	--fileIn (theSRoot + "Scripts\\StokeMenuCreator.ms")

	fileIn (theSRoot + "Scripts\\Magma_ImplementationObjects.ms")
	fileIn (theSRoot + "Scripts\\Genome_MagmaFlowManager.ms")
	try(fileIn (theSRoot + "Scripts\\Genome-AddGenomeModifier.mcr"))catch()
	try(fileIn (theSRoot + "Scripts\\Genome-AddGenomePreset.mcr"))catch()
	
	deleteFile ((getDir #userStartupScripts)+"\\Stoke_RemoveStokeMenu.ms")
	copyFile (theSRoot + "Scripts\\Stoke_RemoveStokeMenu.ms")	((getDir #userStartupScripts)+"\\Stoke_RemoveStokeMenu.ms")

	--Make sure we have enough memory.
	if heapSize < 33554432 do 
	(
		local oldHeap = heapSize 
		heapSize = 33554432
	)

	global removeGenomeCAFromCollapsedNodes, removeGenomeCAFromCollapsedSelNode
	fn removeGenomeCAFromCollapsedNodes =
	(
		--format "Cleaning Up Genome KCM_CAs\n"
		for o in objects where isProperty o #KCM_CA do 
		(
			--format "Removing KCM_CA from Object %\n" o
			try(deleteItem o.baseobject.custAttributes #KCM_CA)catch()
		)
	)
	fn removeGenomeCAFromCollapsedSelNode =
	(
		if selection.count > 0 do 
			for o in selection where isProperty o #KCM_CA do 
				try(deleteItem o.baseobject.custAttributes #KCM_CA)catch()
	)

	callbacks.addScript #filePreSave "::removeGenomeCAFromCollapsedNodes()" id:#GenomeCACleanup
	callbacks.addScript #selectionSetChanged "::removeGenomeCAFromCollapsedSelNode()" id:#GenomeCACleanup	
	callbacks.addScript #postSystemStartup "fileIn (StokeGlobalInterface.HomeDirectory+\"/Scripts/StokeMenuCreator.ms\")" id:#stokestartup
	
	StokeGlobalInterface.MaxMarkerCount = if StokeGlobalInterface.ViewportType == #Legacy then 100000 else 1000000
	local theVal=getIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level"
	if theVal != "" do try(StokeGlobalInterface.LoggingLevel = theVal as name)catch()
	
)
else
	format "--StokeMX.dlo failed to load, could not figure out StokeHome. Not loading the rest...\n"

)

------------------------------------------------------------------------------------------------------------------------

