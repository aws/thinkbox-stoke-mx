-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
--global KrakatoaPRTLoader, KrakatoaDeleteModifier

global StokeWarnAboutOverwriting = execute (getIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Options" "WarnAboutOverwriting")
if StokeWarnAboutOverwriting == OK do StokeWarnAboutOverwriting = true

global StokefieldSimulationShutdownCallbacksArray, StokeFieldSimulationShutdownCallback
if ::StokefieldSimulationShutdownCallbacksArray ==undefined do ::StokefieldSimulationShutdownCallbacksArray= #()
fn StokeFieldSimulationShutdownCallback =
(
	local theCallbacks = for i in ::StokefieldSimulationShutdownCallbacksArray where i.StokeFieldSimPlugin.DoGetFlushFlag() collect i
	if theCallbacks.count > 0 do
	(
		local q = querybox ( theCallbacks.count as string + " Stoke Field Simulation"+ (if theCallbacks.count == 1 then " is" else "s are") +" still caching!\n\nClick [Yes] to DISCARD the unsaved data and close 3ds Max.\nClick [No] to WAIT until all data is cached to disk.") title:"Stoke Asynchronous Cache Still Active!"
		if not q do 
		(
			for i in theCallbacks do
			(
				i.StokeFieldSimPlugin.delegate.FlushCache()
				i.StokeFieldSimPlugin.DoSetFlushFlag false
			)
		)
	)
)

-- Callback struct invoked by StokeFieldSimulator at the end of each frame
struct StokeFieldSimCallback
(
	StokeFieldSimPlugin = undefined,
	StokeFieldSim = undefined,
	
	storageCounter = 0,
	updateCounter = 0,
	
	asyncCacheStart = 0,
	
	fn onFrameUpdate = 
	(
		local theTime = StokeFieldSim.GetCurrentTime()--(StokeFieldSim.GetCurrentTime()/TicksPerFrame as float) as time
	
		format "\t--Stoke Field Sim - Update at Time: %\n" theTime.frame
		
		storageCounter += 1
		if storageCounter == StokeFieldSimPlugin.memoryCacheStep do
		(
			storageCounter = 0				
			if not StokeFieldSimPlugin.useCacheStartTime or theTime >= StokeFieldSimPlugin.CacheStartTime do
				StokeFieldSimPlugin.delegate.AddData StokeFieldSim theTime.frame
		)
		
		if StokeFieldSimPlugin.updateViews and StokeFieldSimPlugin.updateViewsEvery == updateCounter do
		(
			sliderTime = theTime -- -1 --update to previous frame to avoid PRT access error
			updateCounter = 0
		)
		updateCounter += 1
		if ( StokeFieldSimPlugin.onFrameUpdateUI theTime ) then 
		(
			-- Add code for updating animated parameters here.
			at time theTime 
			( 
				--StokeFieldSimPlugin.updateParticleSourceRates particleSources
				--StokeFieldSimPlugin.updateVelocitySourceScales velocityFields
			)
			
			true
		) else false
	),
	
	fn FlushReadyCallback =
	(
		if StokeGlobalInterface.LoggingLevel != #none do
			format "--STOKE FIELD ASYNC DISK CACHE FINISHED SAVING IN % SECONDS.\n\n" ((timestamp()-asyncCacheStart)/1000.0)
		StokeFieldSimPlugin.delegate.SetSerializerCallback undefined
		StokeFieldSimPlugin.DoSetFlushFlag false
		StokeFieldSimPlugin.DoUpdateMemoryCount()

		local theIndex =findItem ::StokeFieldSimulationShutdownCallbacksArray this
		if theIndex > 0 do deleteItem ::StokeFieldSimulationShutdownCallbacksArray theIndex 
			
		-- If there isn't any node pointing at the StokeFieldSim object, we try to reduce the memory usage by resetting the cache. This typically occurs when
		-- the scene is reset while an asynchronous flush is pending.
		if (refs.dependentNodes StokeFieldSimPlugin baseObjectOnly:true).count == 0 do (
			StokeFieldSimPlugin.delegate.ResetCache()
		)
	),
	
	fn FlushFrameReadyCallback theFile =
	(
		if findItem #(#progress, #stats, #debug) StokeGlobalInterface.LoggingLevel > 0 do
			format "  --STOKE FIELD ASYNC DISK CACHE Saved File [%]\n" theFile
		StokeFieldSimPlugin.DoUpdateMemoryCount()
	)	
)--end struct

struct SimEmberInitFunctor
(
	expressionHolder = undefined,
	
	fn initFields fieldCollection context = (
		for n in (expressionHolder.initMagmaHolder.GetNodes()) where ((expressionHolder.initMagmaHolder.GetNodeType n) == "Output") do (
			local chName = expressionHolder.initMagmaHolder.GetNodeProperty n "channelName"
			local chType = expressionHolder.initMagmaHolder.GetNodeProperty n "channelType"
			
			format "\t--Initialized \"%\" with type \"%\"\n" chName chType
			
			local theField = expressionHolder.GetInitialField chName context
			fieldCollection.SetField chName theField
		)
	)
)--end struct

struct SimEmberUpdateFunctor
(
	expressionHolder = undefined,
	
	fn initFields fieldCollection context = (
		for n in (expressionHolder.magmaHolder.GetNodes()) where ((expressionHolder.magmaHolder.GetNodeType n) == "Output") do (
			local chName = expressionHolder.magmaHolder.GetNodeProperty n "channelName"
			local chType = expressionHolder.magmaHolder.GetNodeProperty n "channelType"
			
			if( (fieldCollection.GetFieldByName chName) == undefined ) do (
				format "\t--Uninitialized \"%\" with type \"%\"\n" chName chType
				
				local pmin = (context.GetValue "BoundsMin") as point3
				local pmax = (context.GetValue "BoundsMax") as point3
				local spacing = (context.GetValue "Spacing") as float
				
				if( chType == "Float" or chType == "float32[1]" or chType == "float32" ) then (
					fieldCollection.SetField chName ( SimEmberFactory.CreateConstantField 0.0 pmin pmax spacing name:chName )
				) else if( chType == "Vec3" or chType == "float32[3]") then (
					fieldCollection.SetField chName ( SimEmberFactory.CreateConstantField [0,0,0] pmin pmax spacing name:chName )
				) else (
					format "--ERROR: Invalid type\n"
				)
			)
		)
	),
	
	fn updateFields fieldCollection context timeStepSeconds = (
		format "\t--updateFields!\n"
		
		for i = 1 to fieldCollection.Count do (
			local field = fieldCollection.GetFieldByIndex i
			local fieldName = fieldCollection.GetFieldNameByIndex i
			
			local result = expressionHolder.GetUpdateField fieldCollection fieldName context timeStepSeconds
			
			if result != undefined do
				fieldCollection.SetField fieldName result 
		)
	)
)--end struct


plugin geometry StokeFieldSim
name:"Field Sim"
category:"Stoke"
classid:#(0x3909087c, 0x5f312497)
extends:Stoke_Field_Sim_Base
replaceui:true
(
	local paramsRollout, paramsDisplayRollout, cacheParamsRollout, simRollout, helpRollout, paramsTimingRollout
	local availableChannels = #()
	local availableChannelNames = #()
	local lockUpdates = false
	local RCMenuSourceObject
	local StokeFieldSim_Progress_Cancel = false
	local isSimulating = false
	local theMaxVersion = 0
	local theSim = undefined
	local lastSimHash=0
	local isAsyncFlushActive = false
	local isAsyncFlushCancelled = false
	
	local LastSimErrorID = -100
	local LastSimErrorMsg = undefined
	local LastInitErrorID = -100
	local LastInitErrorMsg = undefined
	
	fn DoUpdateMemoryCount = (
		this.cacheParamsRollout.updateMemoryCount()
	)	
	
	fn DoSetFlushFlag arg = (
		this.isAsyncFlushActive = arg
	)
	
	fn DoGetFlushFlag = (this.isAsyncFlushActive)
	
	on detachedFromNode theNode do (
		-- If we don't have an asynchronous flush pending and this is the last node pointing at the SimEmber object we try to reduce the memory usage
		-- by resetting the cache.
		if not this.isAsyncFlushActive and (refs.dependentNodes this baseObjectOnly:true) == 1 do (
			this.delegate.ResetCache()
		)
	)
	
	fn enableAutoLayout = 
	(
		--Since 3ds Max 2018, the Command Panel can be resized horizontally.
		--Since 3ds Max 2019, the controls in the Command Panel can be auto-positioned and scaled accordingly when the panel is resized
		--In 3ds Max 2020.1, a regression causes all controls to disappear if the auto-layout option is enabled for the rollout!
		--The following code handles all known cases to set the correct value for supported versions.		
		if (maxVersion())[1] > 20000 do -- Only 3ds Max 2019 and higher support the autoLayoutOnResize property! So we have to check against the version value of 2018 (v20) to avoid script errors accessing a non-existing property!
		(
			--the following array contains all scripted rollouts of the UI:
			local theRollouts = #(paramsRollout, paramsDisplayRollout, cacheParamsRollout, paramsTimingRollout, helpRollout, simRollout)
			for aRollout in theRollouts do --loop through all rollouts, and set their autoLayoutOnResize property to the value in the global variable determined in the Stoke startup script:
				setProperty aRollout #autoLayoutOnResize ::Stoke_autoLayoutOnResizeOn
		)
	)		
	
	fn defineRCMenu =
	(
		rcmenu StokeFieldSim_Object_RCMenu
		(
			menuItem mnu_toggleHidden "Hide Object" checked:( if isValidNode RCMenuSourceObject then RCMenuSourceObject.isHidden else false )
			separator sep_10
			menuItem mnu_select "Select Object..."
			
			on mnu_toggleHidden picked do
			(
				if isValidNode RCMenuSourceObject do RCMenuSourceObject.ishidden = not RCMenuSourceObject.ishidden 
			)
			
			on mnu_select picked do
			(
				if isValidNode RCMenuSourceObject do select RCMenuSourceObject
			)
		)	
		StokeFieldSim_Object_RCMenu			
	)		
	
	parameters simParams rollout:simRollout
	(
		holder type:#maxObject
		BoundsMinX type:#float default:-2 ui:spn_boundsMinX animatable:false
		BoundsMaxX type:#float default:2 ui:spn_boundsMaxX animatable:false
		BoundsMinY type:#float default:-2 ui:spn_boundsMinY animatable:false
		BoundsMaxY type:#float default:2 ui:spn_boundsMaxY animatable:false
		BoundsMinZ type:#float default:0 ui:spn_boundsMinZ animatable:false
		BoundsMaxZ type:#float default:4 ui:spn_boundsMaxZ animatable:false
		Gridspacing type:#float default:1.0 ui:spn_gridspacing animatable:false
		
		on boundsMinX set val do 
		(
			if val > boundsMaxX-gridSpacing*2 do boundsMaxX = val + gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on boundsMaxX set val do 
		(
			if val < boundsMinX+gridSpacing*2 do boundsMinX = val - gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on boundsMinY set val do 
		(
			if val > boundsMaxY-gridSpacing*2 do boundsMaxY = val + gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on boundsMaxY set val do 
		(
			if val < boundsMinY+gridSpacing*2 do boundsMinY = val - gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on boundsMinZ set val do 
		(
			if val > boundsMaxZ-gridSpacing*2 do boundsMaxZ = val + gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on boundsMaxZ set val do 
		(
			if val < boundsMinZ+gridSpacing*2 do boundsMinZ = val - gridSpacing*2
			simRollout.updateVoxelGridInfo()
		)
		on gridSpacing set val do simRollout.updateVoxelGridInfo()
		
	)
	
	parameters cacheParams rollout:cacheParamsRollout
	(
		randomID type:#string default:"12345_67890"
		outputPath type:#string default:"" ui:edt_outputPath
		outputVersion type:#string default:"v001" ui:edt_outputVersion
		outputPrefix type:#string default:"StokeField" ui:edt_outputPrefix
		
		saveResultsToDisk type:#boolean default:true ui:chk_saveResultsToDisk
		memoryCacheStep type:#integer default:1 animatable:false ui:spn_memoryCacheStep
		memoryLimit type:#integer default:4096 animatable:false ui:spn_memoryLimit
		threadLimit type:#integer default:0 animatable:false ui:spn_threadLimit
	)
	
	parameters params rollout:paramsRollout
	(
		startTime type:#integer default:0 animatable:false ui:spn_startTime 
		endTime type:#integer default:100 animatable:false ui:spn_endTime
		subSteps type:#integer default:1 animatable:false ui:spn_subSteps
		
		useCacheStartTime type:#boolean default:false animatable:false ui:chk_useCacheStartTime
		cacheStartTime type:#integer default:0 animatable:false ui:spn_cacheStartTime
		
		updateViews type:#boolean default:false animatable:false ui:chk_updateViews
		updateViewsEvery type:#integer default:5 ui:spn_updateViewsEvery
		
		useSolver type:#integer default:2 ui:ddl_usesolver
		
		
		--on rate set val do paramsRollout.updateUI()
		on startTime set val do if startTime > endTime do endTime = startTime
		on endTime set val do if startTime > endTime do startTime = endTime
	)
	
	
	parameters paramsDisplay rollout:paramsDisplayRollout
	(
		ViewVectorNormalize type:#boolean default:false ui:chk_ViewVectorNormalize
		viewVectorScale type:#float default:1.0 ui:spn_viewVectorScale 
		iconSize type:#float default:30.0 ui:spn_iconSize
		
		minManipulatorSize type:#float default:0.1 animatable:false ui:spn_minManipulatorSize
		maxManipulatorSize type:#float default:10.0 animatable:false ui:spn_maxManipulatorSize		
		
		forceUIUpdates type:#float default:0 ui:spn_forceUIUpdates	--parameter used to force UI updates as a replacement of time callbacks that broke in 3ds Max 2019.3
		
		on viewVectorScale set val do this.delegate.viewportVectorScale = val
		on ViewVectorNormalize set val do this.delegate.viewportVectorNormalize = val
		
		on iconSize set val do 
		(
			this.delegate.iconSize = val
			redrawViews()
		)
		
		on forceUIUpdates get val do	--update any UI parameters that don't have auto-update capabilities
		(
			cacheParamsRollout.updateMemoryCount()
			val
		)		
	)
	

	fn getZeros theNumber =
	(
		theCount = (theNumber as string).count
		if theCount < 999 then
			substring "000" 1 (3-theCount)
		else
			""	
	)		
	
	fn addCommas txt =
	(
		if matchPattern txt pattern:"*L" do txt = substring txt 1 (txt.count-1)
		if matchPattern txt pattern:"*P" do txt = substring txt 1 (txt.count-1)
		theDot = findString txt "."
		if theDot == undefined then 
		(
			newTxt = "" 
			
			theDot = txt.count
		)	
		else 
		(
			newTxt = substring txt theDot -1
			theDot -= 1
		)
		cnt = 0
		for i = theDot to 1 by -1 do
		(
			cnt +=1
			newTxt = txt[i] + newTxt
			if cnt == 3 and i > 1 and txt[i-1] != "-" do 
			(
				newTxt = "," + newTxt
				cnt = 0
			)	
		)
		newTxt
	)	


	fn getCachePath =
	(
		local theString = outputPath
		if outputPath == "" do outputPath = "$default\\"
		local theDefaultPath = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\Cache\\"
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
	
	fn createNewVersion theVersion =
	(
		local theCachePath = getCachePath()
		makedir (theCachePath+"StokeField_"+randomID+"\\"+theVersion) all:true		
		outputVersion = theVersion
		cacheParamsRollout.setCachePath()
	)
	
	fn GetDirNames =
	(
		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + "StokeField_"+randomID+"\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		theDirNames
	)
	
	fn getNewVersionFolder =
	(
		local theDirNames = GetDirNames()
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
	
	fn createVersionRCMenu =
	(
		global StokeFieldSim_Presets_RCMenu
		local txt = "rcmenu StokeFieldSim_Presets_RCMenu\n(\n"
		local nextVersion = getNewVersionFolder()
		local theCachePath = getCachePath()	
		local theDirNames = GetDirNames()
		txt += "menuItem mnu_NextVersion \"New Version ["+nextVersion+"]\" \n"
		txt += "on mnu_NextVersion picked do (selection[1].createNewVersion \""+ nextVersion +"\" )\n"
		txt += "separator sep_10\n"
			
		for f in theDirNames do
		(
			txt += "menuItem mnu_"+f+" \""+ f + "\" \n"
			txt += "on mnu_"+f+" picked do (selection[1].createNewVersion \""+ f +"\" )\n"
		)
			
		txt += ")\n"
		execute txt
	)
	
	fn createPrefixRCMenu =
	(
		if selection[1] == undefined or classof selection[1].baseobject != StokeFieldSim do return false
		global StokeFieldSim_Presets_RCMenu
		local txt = "rcmenu StokeFieldSim_Presets_RCMenu\n(\n"
			
		txt += "menuItem mnu_defaultName \"Default Name 'StokeField'\" \n"
		txt += "on mnu_defaultName picked do (selection[1].outputPrefix = \"StokeField\")\n"
		txt += "separator sep_10\n"
			
		txt += "menuItem mnu_objectName \"Object Name '"+selection[1].name+"'\" \n"
		txt += "on mnu_objectName picked do (selection[1].outputPrefix = selection[1].name)\n"
			
		txt += ")\n"
		execute txt		
		true
	)
	
	fn getListViewSelection lv =
	(
		try
			sort (for i = 1 to lv.items.count where lv.items.item[i-1].Selected collect i)
		catch
			#()
	)	
	
	fn setListViewSelection lv theSel =
	(
		try
		(
			for i = 1 to lv.items.count do lv.items.item[i-1].Selected = false
			for i in theSel do lv.items.item[i-1].Selected = true
		)catch()
	)	
	
	fn createPresetsRCMenu type:#renderpercent =
	(
		if selection[1] == undefined or classof selection[1].baseobject != StokeFieldSim do return false
		local isTime = false
		local isInDelegate = false
		case type of
		(
			#gridSpacing: (
						presetName = "gridSpacing"
						theParameter = "gridSpacing"
						isTime = false
			)
			#startTime : (
						presetName = "startTime"
						theParameter = "startTime"
						isTime = true
					)	
			#endTime : (
						presetName = "endTime"
						theParameter = "endTime"
						isTime = true
					)	
			#cacheStartTime: (					
						presetName = "cacheStartTime"
						theParameter = "cacheStartTime"		
						isTime = true				
					)
			#subSteps: (
						presetName = "subSteps"
						theParameter = "subSteps"
					)
			#viewPercentage: (
						presetName = "viewPercentage"
						theParameter = "viewPercentage"				
				)
			#viewLimit: (
						presetName = "viewLimit"
						theParameter = "viewLimit"				
				)
			#iconSize: (
						presetName = "iconSize"
						theParameter = "iconSize"				
				)		
			#lifeSpan: (
						presetName = "lifeSpan"
						theParameter = "lifeSpan"				
				)	
			#lifeSpanVar: (
						presetName = "lifeSpanVar"
						theParameter = "lifeSpanVar"				
				)	
			#updateViewsEvery: (
						presetName = "updateViewsEvery"
						theParameter = "updateViewsEvery"				
			)
			#viewVectorScale : (
						presetName = "viewVectorScale"
						theParameter = "viewVectorScale"					
			)
			#perObjectRate : (
						presetName = "perObjectRate"
						theParameter = "perObjectRate"					
			)
			#memoryCacheStep: (
						presetName = "memoryCacheStep"
						theParameter = "memoryCacheStep"
			)
			#memoryLimit: (
						presetName = "memoryLimit"
						theParameter = "memoryLimit"
			)
			#threadLimit: (
						presetName = "threadLimit"
						theParameter = "threadLimit"
			)
			#viewportreduce: (
						presetName = "viewportreduce"
						theParameter = "viewportreduce"
						isTime = false
						isInDelegate = true
			)
			#minManipulatorSize: (
						presetName = "minManipulatorSize"
						theParameter = "minManipulatorSize"
						isTime = false
						isInDelegate = false
			)
			#maxManipulatorSize: (
						presetName = "maxManipulatorSize"
						theParameter = "maxManipulatorSize"
						isTime = false
						isInDelegate = false
			)			
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#gridSpacing: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
				#startTime : #(0,1)
				#endTime : #(1,30,50,60,100)
				#subSteps : #(1,2,4,6,8,10)
				#emitEndTime : #(1,30,50,60,100)
				#cacheStartTime: #(0,1)
				#rate: #(10,100,1000,10000)
				#JitterRadius: #(0.01,0.1,1.0)
				#randomSeed: #(1,10000,12345)
				#VelocityScale: #(0.1,0.5,1.0,2.0)
				#gridSize: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
				#gridPadding: #(3,5,8,10)
				#viewPercentage: #(1.0,10.0,50.0,100.0)
				#viewLimit: #(10,100,1000,2000,5000)
				#iconSize: #(1.0,10.0,30.0,50.0,100.0)
				#lifespan: #(10,20,30,40,50,60,70,80,90,100)
				#lifespanVar: #(0,5,10,15,20,25,30)
				#updateViewsEvery: #(1,2,3,5,10,20)
				#viewVectorScale: #(0.01,0.03,0.04,0.05,0.1,0.3,0.5,1.0,2.0,10.0,24.0,25.0,30.0)
				#perObjectRate: #(100,1000,2000,5000,10000,100000,1000000)
				#memoryCacheStep: #(1,2,3,5,10,15,20)
				#memoryLimit: #(1024,2048,3072,4096,5120,6144,7168,8192,12288,16384,24576,32768,65536)
				#threadLimit: #(0,1,2,4,8,12,16,24,32)
				#perObjectJitter: #(1.0,10.0,100.0,10000.0)
				#perObjectVolumeSpacing: #(1.0,2.0,5.0,10.0)
				#viewportreduce: #(0,1,2,3,5,10,20,50,100)
				#minManipulatorSize: #(0.1,1.0,2.0,5.0,10.0)
				#maxManipulatorSize: #(5.0,10.0,20.0,50.0)				
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("selection[1]." + theDelText + theParameter)
		global StokeFieldSim_Presets_RCMenu
		local txt = "rcmenu StokeFieldSim_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try(selection[1].paramsRollout.updateUI())catch()\n"
		txt+= "try(selection[1].cacheParamsRollout.updateUI())catch()\n"
		--txt+= "try(selection[1].paramsVelocityFieldRollout.updateUI())catch()\n"
		txt+= "try(selection[1].paramsDisplayRollout.updateUI())catch()\n"
		
		txt+= ")\n"
		
		if isTime do
		(
			txt += "menuItem mnu_StartFrame \""+ (animationrange.start.frame as integer) as string+" - Start Frame \"\n" 
			txt += "on mnu_StartFrame picked do (selection[1]."+  theParameter +" = "+ (animationrange.start.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_CurrentFrame \""+ (sliderTime.frame as integer) as string+" - Current Frame\"\n" 
			txt += "on mnu_CurrentFrame picked do (selection[1]."+ theParameter +" = "+ (sliderTime.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_EndFrame \""+ (animationrange.end.frame as integer) as string+" - End Frame\"\n" 
			txt += "on mnu_EndFrame picked do (\nselection[1]."+ theParameter +" = "+ (animationrange.end.frame as integer) as string +"\nupdateUI())\n"
			
			txt += "separator spr_10\n"
		)	
		if type == #gridSpacing do
		(
			local theDefaultValue = (length ([boundsMaxX,boundsMaxY,boundsMaxZ ]-[boundsMinX,boundsMinY,boundsMinZ])) / 50.0
			txt += "menuItem mnu_defaultSpacing \"Default "+ theDefaultValue as string+"\"\n" 
			txt += "on mnu_defaultSpacing picked do (\nselection[1]."+ theParameter +" = "+ theDefaultValue as string +"\nupdateUI())\n"
			txt += "separator spr_15\n" 			
		)
			
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( selection[1]."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)		
	
	fn onFrameUpdateUI theTime updateDisplay:false = 
	(
		if updateDisplay do sliderTime = theTime
		
		local progressValue = 0.0
		
		-- When we have preroll frames, we could get negative progress without clamping it.
		if theTime > startTime do 
			progressValue = (100.0 * (theTime-startTime) / (endTime-startTime) ) as integer
		
		if theMaxVersion > 12000 do windows.processPostedMessages()
		
		--paramsRollout.prg_progress.value = progressValue
		paramsRollout.btn_cancel.text = "STOP | Frame " + (theTime.frame as integer) as string +" ("+ progressValue as string + "%)"
		cacheParamsRollout.updateMemoryCount()
		if theMaxVersion < 13000 do StokeFieldSim_Progress_Cancel = not (progressUpdate progressValue)
		not StokeFieldSim_Progress_Cancel
	)
	
	fn getHashSum =
	(
		local thePropString = "" as stringStream
		for p in getPropNames this where findItem #(#memoryCacheStep,#memoryLimit, #threadLimit, #subSteps, #updateViews, #updateViewsEvery,#rate,#endtime,#viewPercentage,#viewLimitEnabled,#viewLimit,#ViewVectorNormalize,#viewVectorScale,#iconSize,#velocityScales,#VelocityScale) p == 0 do
		(
			format "%\n" (getProperty this p) to:thePropString 
		)
		(dotNetObject "System.String" thePropString).GetHashCode()
	)
	
	fn collectVelocitySources = 
	(
		local result = #()
	)
	
	fn collectParticleSources = 
	(
		result = #()
	)
	
	fn collectParticleChannels particleSources = 
	(
		local result = #()
	)
	
	-- Updates the (potentially animated) seeding rates for particles sources. Should be called within an `at time #` block.
	fn updateParticleSourceRates particleSources = 
	(
		local activeEmittersCount = 0
		local totalRates = 0
		
		for i = 1 to particleSources.count where particleSources[i] != undefined do 
		(
			activeEmittersCount += 1
			totalRates += distRates[i]
		)
		
		for i = 1 to particleSources.count where particleSources[i] != undefined do 
		(
			local theRate = if ( distSeedAsRate[i] == true ) then ( -1 ) else 
			(
				case rateMode of
				(
					1: if totalRates != 0 then ((floor ( 0.5 + (((distRates[i] as float)/totalRates)*rate))) as integer) else 0
					2:(floor ((1.0*rate/activeEmittersCount))+0.5) as integer
					3: rate
					4: distRates[i]
					default: 0
				)
			)
			
			-- We set the rate to 0 outside of emitEndTime
			if useEmitEndTime and currentTime > emitEndTime then
				theRate = 0
			else if theRate > 0 do -- We divide the rate by the number of substeps (unless rate is -1 which is a special value)
				theRate = theRate / subSteps
			
			particleSources[i].RandomSeed = randomSeed + (if incrementRandomSeed then (currentTime) as integer else 0) + i
			particleSources[i].GeneratorRate = theRate
			--format "%: %\n" currentTime particleSources[i].GeneratorRate
			
		)
		OK
	)
	
	-- Updates the (potentially animated) scale for velocity sources. Should be called within an `at time #` block.
	fn updateVelocitySourceScales velocitySources = 
	(
		for i = 1 to velocitySources.count where velocitySources[i] != undefined do 
			velocitySources[i].VelocityScale = VelocityScales[i]
		OK
	)
	
	rollout helpRollout "Help" rolledup:true
	(
		label lbl_about10 "FIELD SIMULATOR" align:#center offset:[0,-3]
		label lbl_about01 "STOKE Particle Reflow Tools" align:#center offset:[0,-1]
		edittext edt_version "" align:#center readOnly:true align:#center offset:[-2,0] fieldwidth:120
		
		dotNetControl btn_openOnlineHelp "Button" text:"Open Online Help..." width:152 height:22 align:#center offset:[0,0] enabled:true
		label lbl_logLevel "Log Level:" across:2 align:#left offset:[-5,0]
		dropdownlist ddl_loggingLevel items:#("None","Error","Warning","Progress","Stats","Debug") width:98 align:#right offset:[9,-3]
		
		on ddl_loggingLevel selected itm do
		(
			StokeGlobalInterface.LoggingLevel = ddl_loggingLevel.selected as name
			setIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level" (StokeGlobalInterface.LoggingLevel as string)
		)
		
		on btn_openOnlineHelp click arg do 
		(
			shellLaunch "https://docs.thinkboxsoftware.com/products/stoke/2.3/1_Documentation/manual/fields/stoke-mx-field-simulator-object.html" ""
		)
		
		fn updateLayout =
		(
			btn_openOnlineHelp.FlatStyle = btn_openOnlineHelp.FlatStyle.System
			btn_openOnlineHelp.width = helpRollout.width-8
			btn_openOnlineHelp.pos.x = 4
			ddl_loggingLevel.width = helpRollout.width-64
			ddl_loggingLevel.pos.x = 60
		)		
		
		on helpRollout open do
		(	
			enableAutoLayout()
			updateLayout()

			edt_version.text = try("Version " + StokeGlobalInterface.Version)catch("")
			
			ddl_loggingLevel.selection = findItem (for i in ddl_loggingLevel.items collect i as name) StokeGlobalInterface.LoggingLevel
		)
	)
	
	
	/*rollout presetsRollout "Presets" rolledup:true
	(
		button btn_loadPreset "LOAD Preset" across:2 offset:[-5,0]
		button btn_savePreset "SAVE Preset" offset:[5,0] 
		on btn_loadPreset pressed do
		(
			global SimEmberMX_PresetDialogMode = #load
			global SimEmberMX_PresetObject = this
			local rootFolder = StokeGlobalInterface.EmberHome
			if rootFolder != undefined do 
				fileIn (rootFolder + "Scripts\\SimEmberPresetsManager.ms")
		)
		on btn_savePreset pressed do
		(
			global SimEmberMX_PresetDialogMode = #save
			global SimEmberMX_PresetObject = this
			local rootFolder = StokeGlobalInterface.EmberHome
			if rootFolder != undefined do 
				fileIn (rootFolder + "Scripts\\SimEmberPresetsManager.ms")
		)		
	)*/
	
	rollout simRollout "Stoke Field Sim Parameters"
	(
		progressbar prg_initSimError value:100 color:green width:8 height:30 offset:[-12,-3] align:#left across:3
		dotNetControl btn_initSim "Button" text:"Open INITIAL Flow..." align:#center width:136 height:30 offset:[-3,-3] 
		button btn_initSimPreset ">" align:#right width:15 height:30 offset:[12,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2)

		progressbar prg_updateSimError value:100 color:green width:8 height:30 offset:[-12,-3] align:#left across:3
		dotNetControl btn_updateSim "Button" text:"Open SIMULATION Flow..." align:#center width:136 height:30 offset:[-3,-3] 
		button btn_updateSimPreset ">" align:#right width:15 height:30 offset:[12,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2)
		
		group "World Space Simulation Grid "
		(
			checkbutton chk_enableGridManipulator ">Enable Grid Manipulator" width:146 offset:[0,-3] checked:manipulateMode align:#center
			
			spinner spn_boundsMinX "X" range:[-10000,10000,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxX "" range:[-10000,10000,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			
			spinner spn_boundsMinY "Y" range:[-10000,10000,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxY "" range:[-10000,10000,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			 
			spinner spn_boundsMinZ "Z" range:[-10000,10000,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxZ "" range:[-10000,10000,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			
			spinner spn_gridSpacing "Spacing " range:[0.0001,10000,1.0] fieldwidth:43 enabled:true across:2 type:#worldunits align:#right offset:[57,0]
			button btn_gridSpacing_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Grid voxel size in world units."
			
			edittext edt_voxelres "" offset:[5,0] align:#right width:150 readonly:true
		)
		
		dotNetControl btn_centerGridToIcon	 "Button" text:"Grid To Icon" width:77 align:#left offset:[-9,-3] across:2 height:20 tooltip:"Move the current Grid's Center to the Icon's Position."
		dotNetControl btn_centerIconToGrid	 "Button" text:"Icon To Grid" width:77 align:#right offset:[9,-3] height:20 tooltip:"Move the Icon to the Center of the current Grid ."
		dotNetControl btn_centerGridBaseToIcon "Button" text:"Base To Icon" width:77 align:#left offset:[-9,-5] across:2 height:20 tooltip:"Move the current Grid's Base Center to the Icon's Position."
		dotNetControl btn_centerIconToGridBase "Button" text:"Icon To Base" width:77 align:#right offset:[9,-5] height:20 tooltip:"Move the Icon to the Center of the current Grid's Base."
		
		dotNetControl btn_dependentNodes "Button" text:"Select Dependent Object..." width:154 height:20 align:#center offset:[0,-2] tooltip:"Select an object that depends on this Field Magma object..."
		
		on btn_dependentNodes click arg do
		(
			global StokeField_SelectDependentObject_RCMenu 
			local txt = "" as stringStream
			format "rcmenu StokeField_SelectDependentObject_RCMenu (\n" to:txt
			local theDepNodes = (refs.dependentNodes (refs.dependentNodes this)[1])
			if theDepNodes.count > 0 then
			(
				for i = 1 to theDepNodes.count do
				(
					format "menuItem itm_% \"Select '%' (%)\"" i theDepNodes[i].name (classof theDepNodes[i]) to:txt
					format "on itm_% picked do select (getNodeByName \"%\") \n" i theDepNodes[i].name  to:txt
				)
			)
			else
			(
				format "menuItem itm_1 \"No Dependent Objects Found!\" \n"  to:txt
			)
			format ")\n" to:txt
			execute (txt as string)
			popupmenu StokeField_SelectDependentObject_RCMenu pos:mouse.screenpos
		)		
		
		on btn_centerGridToIcon click arg do
		(
			theIconPos = selection[1].pos
			theBoxDiagonal = (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			boundsMaxX = theIconPos.x + theBoxDiagonal.x
			boundsMaxY = theIconPos.y + theBoxDiagonal.y
			boundsMaxZ = theIconPos.z + theBoxDiagonal.z
			boundsMinX = theIconPos.x - theBoxDiagonal.x
			boundsMinY = theIconPos.y - theBoxDiagonal.y
			boundsMinZ = theIconPos.z - theBoxDiagonal.z
			redrawViews()
		)
		on btn_centerGridBaseToIcon click arg do
		(
			theIconPos = selection[1].pos
			theBoxDiagonal = (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			boundsMaxX = theIconPos.x + theBoxDiagonal.x
			boundsMaxY = theIconPos.y + theBoxDiagonal.y
			boundsMaxZ = theIconPos.z + theBoxDiagonal.z*2
			boundsMinX = theIconPos.x - theBoxDiagonal.x
			boundsMinY = theIconPos.y - theBoxDiagonal.y
			boundsMinZ = theIconPos.z
			redrawViews()
		)	
		on btn_centerIconToGrid click arg do
		(
			selection[1].pos = [boundsMinX,boundsMinY,boundsMinZ] + (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			redrawViews()
		)		
		on btn_centerIconToGridBase click arg do
		(
			local theBox = [boundsMinX,boundsMinY,boundsMinZ] + (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			selection[1].pos = [theBox.x,theBox.y, boundsMinZ]
			redrawViews()
		)			
		
		on chk_enableGridManipulator changed state do manipulateMode = state
		
		on btn_initSim click arg do (
			local theRollout = ::StokeFieldSimInit_ImplementationObject.openEditor holder.initMagmaHolder this 
			--update the error display in the Editor if any
			theRollout.updateErrorLog NodeID:LastInitErrorID ErrorMessage:LastInitErrorMsg
		)
		
		on btn_updateSim click arg do (
			local theRollout = ::StokeFieldSimSim_ImplementationObject.openEditor holder.magmaHolder this 
			theRollout.updateErrorLog NodeID:LastSimErrorID ErrorMessage:LastSimErrorMsg
		)
		
		fn createPresetMenu mode =
		(
			local subFolder = if mode == #init then "SIM Initial" else "SIM Simulation"
			local theUserFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\MagmaFlows\\"
			makedir (theUserFolder + subFolder) all:true
			local theFiles = getFiles (theUserFolder + subFolder +"\\*.MagmaScript")
			--local theArray = join #("<None>") (for f in initSims collect (getFileNameFile f))
			--ddl_updateSims.items = join #("<None>") (for f in updateSims collect (getFileNameFile f))
			local ss = stringStream ""
			format "global StokeFieldSimFlowPresetMenu \n" to:ss
			format "rcmenu StokeFieldSimFlowPresetMenu \n(\n" to:ss

			format "menuItem mnu_openFDV \"OPEN Stoke Field Data VIEWER...\"\n" to:ss
			format "on mnu_openFDV picked do \n" to:ss
			format "try(fileIn (StokeGlobalInterface.HomeDirectory+\"/Scripts/Stoke_FieldDataViewer.ms\"))catch()\n" to:ss
	--		format "separator sep_10\n" to:ss
			format "menuItem mnu_openFDP \"OPEN Stoke Field Data PROBE...\"\n" to:ss
			format "on mnu_openFDP picked do \n" to:ss
			format "Macros.run \"Stoke\" \"StokeFieldProbe\"\n" to:ss
	--		format "separator sep_20\n" to:ss
			format "menuItem mnu_openFDE \"OPEN Stoke Field Data EXPORTER...\"\n" to:ss
			format "on mnu_openFDE picked do \n" to:ss
			format "Macros.run \"Stoke\" \"StokeFieldDataExporter\"\n" to:ss
			format "separator sep_30\n" to:ss			
			
			format "menuItem mnu_explore \"EXPLORE Magma Presets Folder...\"\n" to:ss
			format "separator sep_50\n" to:ss
			format "on mnu_explore picked do (shelllaunch @\"%\" \"\")\n"  (theUserFolder+subFolder) to:ss
			
			for i = 1 to theFiles.count do
			(
				format "menuItem mnu_% \"%\"\n" i (getFileNameFile theFiles[i]) to:ss
				format "on mnu_% picked do (\n" i  to:ss
				if mode == #init then
				(
					format "local theMagma = (modPanel.getCurrentObject()).holder.initMagmaHolder\n" to:ss
					format "theMagma.Reset()\n" to:ss
					format "local theEditor = ::StokeFieldSimInit_ImplementationObject.openEditor theMagma (modPanel.getCurrentObject()) offscreen:true\n" to:ss
				)
				else
				(
					format "local theMagma = (modPanel.getCurrentObject()).holder.magmaHolder\n" to:ss
					format "theMagma.Reset()\n" to:ss
					format "local theEditor = ::StokeFieldSimSim_ImplementationObject.openEditor theMagma (modPanel.getCurrentObject()) offscreen:true\n" to:ss
				)
				format "::StokeFieldMFEditor_Functions.loadPreset @\"%\" theMagma (modPanel.getCurrentObject())\n" thefiles[i] to:ss
				format "theEditor.exposeControlsToModifier()\n" to:ss
				format "destroyDialog theEditor\n" to:ss
				format ")\n" to:ss
			)
			format ")\n" to:ss
			::StokeFieldSimFlowPresetMenu = execute (ss as string)
		)
		
		fn addCommas txt =
		(
			if matchPattern txt pattern:"*L" do txt = substring txt 1 (txt.count-1)
			if matchPattern txt pattern:"*P" do txt = substring txt 1 (txt.count-1)
			theDot = findString txt "."
			if theDot == undefined then 
			(
				newTxt = "" 
				
				theDot = txt.count
			)	
			else 
			(
				newTxt = substring txt theDot -1
				theDot -= 1
			)
			cnt = 0
			for i = theDot to 1 by -1 do
			(
				cnt +=1
				newTxt = txt[i] + newTxt
				if cnt == 3 and i > 1 and txt[i-1] != "-" do 
				(
					newTxt = "," + newTxt
					cnt = 0
				)	
			)
			newTxt
		)		
		
		
		fn updateVoxelGridInfo =
		(
			theGridSize = ([boundsMaxX,boundsMaxY,boundsMaxZ ]-[boundsMinX,boundsMinY,boundsMinZ]) / gridSpacing
			local txt = (theGridSize.x as integer) as string + "x" +(theGridSize.y as integer) as string + "x" +(theGridSize.z as integer) as string  + " | " + addCommas ((theGridSize.x as integer * theGridSize.y as integer * theGridSize.z as integer) as string)
			edt_voxelres.text = txt
		)

		on btn_initSimPreset pressed do
		(
			createPresetMenu #init 
			popupMenu ::StokeFieldSimFlowPresetMenu pos:mouse.screenpos
		)
		on btn_updateSimPreset pressed do
		(
			createPresetMenu "SIM Simulation"
			popupMenu ::StokeFieldSimFlowPresetMenu pos:mouse.screenpos
		)
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
			updateVoxelGridInfo()
		)
		on btn_gridSpacing_Preset pressed do popupPresetMenu #gridSpacing
		on btn_gridSpacing_Preset rightclick do popupPresetMenu #gridSpacing
			
		fn updateLayout =
		(
			btn_dependentNodes.FlatStyle = btn_centerIconToGrid.FlatStyle = btn_centerIconToGridBase.FlatStyle = btn_centerGridBaseToIcon.FlatStyle = btn_centerGridToIcon.FlatStyle =  btn_initSim.FlatStyle = btn_updateSim.FlatStyle = btn_initSim.FlatStyle.System
			btn_initSim.width = btn_updateSim.width = simRollout.width-32
			btn_initSim.pos.x = btn_updateSim.pos.x = 12
			
			chk_enableGridManipulator.pos.x = 8
			try(chk_enableGridManipulator.width = simRollout.width-16)catch()
			
			spn_boundsMinX.pos.x = spn_boundsMinY.pos.x = spn_boundsMinZ.pos.x = simRollout.width-90
			spn_GridSpacing.pos.x = simRollout.width-32
			
			btn_dependentNodes.width = simRollout.width-6
			btn_dependentNodes.pos.x = 3
			
			
			btn_centerIconToGrid.width = btn_centerIconToGridBase.width = btn_centerGridBaseToIcon.width = btn_centerGridToIcon.width = (simRollout.width-6)/2
			
			btn_centerIconToGrid.pos.x = btn_centerIconToGridBase.pos.x = 3+(simRollout.width-6)/2
			btn_centerGridBaseToIcon.pos.x = btn_centerGridToIcon.pos.x = 3
			
		)		
			
		on simRollout open do
		(
			enableAutoLayout()
			updateLayout()
			updateVoxelGridInfo()
			--updatePresetLists()
		)
		
	)
	
	rollout cacheParamsRollout "Saving and Caching"
	(
		edittext edt_outputPath offset:[-15,-3] fieldwidth:140 align:#left across:2
		button btn_getOutputPath "..." width:16 height:17 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,11,11,11,11) tooltip:"Click to open a menu with various Base Output Path operations...\n\nEnter a user-defined path in the text field to the left - either using an explicit path or the following symbolic tokens:\n\n$default = C:\\Users\\<UserName>\\AppData\\Local\\Thinkbox\\StokeField\\Cache\\ \n$scene = 3ds Max Scene File Name\n$user = System User Name"
		
		edittext edt_outputVersion offset:[-15,-3] fieldwidth:60 align:#left across:4
		button btn_outputVersion ">"  width:16 height:17 align:#center offset:[7,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Click to create a new Version folder or select an existing Version folder..."
		
		edittext edt_outputPrefix offset:[12,-3] fieldwidth:60 align:#center 
		button btn_outputPrefix ">"  width:16 height:17 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"" enabled:false

		edittext edt_driveinfo offset:[-3,-3] fieldwidth:158 align:#center readonly:true		
		
		group "Cache Options"		
		(
			progressbar prg_savingStatus across:3 width:11 height:22 align:#left offset:[-6,-4] color:(white*0.5) value:100
			checkbutton chk_saveResultsToDisk ">Use Disk Cache" offset:[-3,-4] align:#center width:119 height:22 across:2 tooltip:"If checked, the simulation will be saved to a VDB file sequence asynchronously during the simulation.\n\nThe disk cache will be used to restore the memory cache on scene load.\n\nThe resulting sequence can also be loaded using a Stoke Field Loader - see the [+] button to the right."
			button btn_createVDBLoader "+" width:18 height:22 align:#right offset:[7,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,13,13,21,21) tooltip:"Click to create a new Stoke Field Loader object with the current VDB sequence."
			
			button btn_stopAsyncCache "CANCEL SAVING" offset:[-4,-27] visible:false align:#center width:119 height:22 tooltip:"Click to cancel the asynchronous saving of VDB files by the background thread."
			
			spinner spn_memoryCacheStep "Cache Nth " fieldwidth:40 range:[1,20,0] type:#integer align:#right offset:[56,-3] across:2 tooltip:"Defines the Memory Cache step.\n\nStokeFieldSim can interpolate the velocities by ID between frames, so storing a sub-set of frames is often enough to get a fair idea of the simulation's look."
			button btn_memoryCacheStep_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Memory Cache step.\n\nStokeFieldSim can interpolate the velocities by ID between frames, so storing a sub-set of frames is often enough to get a fair idea of the simulation's look."

			spinner spn_threadLimit "Cache Threads " fieldwidth:40 range:[0,sysinfo.cpucount,0] type:#integer align:#right offset:[56,-3] across:2 tooltip:"Defines the Maximum Number of Threads to use for Saving of VDB files in the Disk Cache.\n\nWhen set to 0, the Number of CPUs/Cores will be used."
			button btn_threadLimit_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Maximum Number of Threads to use for Saving of VDB files in the Disk Cache.\n\nWhen set to 0, the Number of CPUs/Cores will be used."
			
			spinner spn_memoryLimit "Memory Limit " fieldwidth:40 range:[0,1000000,1024] type:#integer align:#right offset:[56,-3] across:2 tooltip:"Defines the Memory Cache Size in MegaBytes."
			button btn_memoryLimit_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Memory Cache Size in MegaBytes."
			
			edittext edt_memoryUsed "" text:"0" readOnly:true fieldwidth:130 align:#left offset:[-10,-3] across:2
			button btn_clearParticleCache "X" width:18 height:17 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,14,14,14,14) tooltip:"Click for a menu with options to CLEAR the cache or save it to an alternate location."
		)
		bitmap btm_memoryCacheMap width:154 height:20 align:#center offset:[0,-4] 
		
		fn updateAsyncControlsUI =
		(
			edt_outputPath.enabled = btn_getOutputPath.enabled = edt_outputVersion.enabled = btn_outputVersion.enabled = edt_outputPrefix.enabled = btn_outputPrefix.enabled = not isAsyncFlushActive 
			chk_saveResultsToDisk.visible = not isAsyncFlushActive 
			btn_stopAsyncCache.enabled = not isAsyncFlushCancelled
			btn_stopAsyncCache.text = if isAsyncFlushCancelled then "CANCELLING..." else "CANCEL SAVING" 
			btn_stopAsyncCache.visible = isAsyncFlushActive 
		)		
		
		fn updateMemoryCacheGraph =
		(
			local st = timestamp()
			local bitmapWidth = btm_memoryCacheMap.width
			if bitmapWidth == undefined do bitmapWidth = 154
			local theSimLength = ((endTime-startTime)+1)
			local numRows = floor (theSimLength/152)
			local bitmapHeight = amax #(amin #(numRows*4, 30), 15) 
			local theBitmap = bitmap bitmapWidth bitmapHeight color:(white*0.5)
			
			local theMap = bitmap bitmapWidth (amax #(1,(numRows+1))) color:(white*0.5)
			local x = 0
			local y = 0
			--local theTimes = for i in this.delegate.GetCacheTimes() collect (i.frame as integer)
			local theTimes = for i in this.delegate.GetCacheTimes() collect (i as integer)
			--local theMemTimes = for i in this.delegate.GetMemoryCacheTimes() collect (i.frame as integer)
			local theMemTimes = for i in this.delegate.GetMemoryCacheTimes() collect (i as integer)
			local greenColor = green*0.85
			local blueColor = color 0 125 255
			local redColor = red*0.85
			local cnt = 0
			if theTimes.count > 0 then
			(
				for t = startTime to endTime do
				(
					cnt += 1
					local theColor = (if findItem theTimes t > 0 then (if findItem theMemTimes t > 0 then greenColor else blueColor) else redColor)
					if cnt == 10 do 
					(
						cnt = 0
						theColor *= 0.85
					)
					if t == currentTime do 
					(
						theColor += color 100 100 100
						if theColor.r > 255 do theColor.r = 255
						if theColor.g > 255 do theColor.g = 255
						if theColor.b > 255 do theColor.b = 255
					)
					setPixels theMap [x+3,y] #(theColor)
					x+=1
					if x == 150 do 
					(
						x = 0
						y+=1
					)
				)
				copy theMap theBitmap
			)
			else prg_savingStatus.color = white*0.5
				
			btm_memoryCacheMap.height = bitmapHeight+5
			btm_memoryCacheMap.bitmap = theBitmap
			updateAsyncControlsUI()
			--format "%\n" (timestamp()-st)
		)		
		
		fn getFreeSpaceOnDisk thePath =
		(
			local dnc = dotNetClass "System.IO.DriveInfo"
			local theDrives = dnc.GetDrives()
			local ss = "" as stringStream
			for aDrive in theDrives do
			(
				if matchPattern thePath pattern:(aDrive.RootDirectory.Name+"*") do
					format "% % GB, %, %  " aDrive.Name (addCommas ((aDrive.AvailableFreeSpace/1024.0/1024/1024) as string)) aDrive.DriveFormat (aDrive.DriveType.ToString())   to:ss
			)
			if ss as string == "" do ss = "Disk Space Data Not Available"
			edt_driveinfo.text = ss as string
		)
		
		fn updateMemoryCount =
		(
			theUsage = this.delegate.SerializeQueueUsageMB
			edt_memoryUsed.text = "Used:" + this.delegate.SequenceCacheUsageMB as string + "MB | " + theUsage as string + "MB"
			this.delegate.SequenceCacheCapacityMB = memoryLimit - theUsage
			this.delegate.SerializeQueueCapacityMB = theUsage
			prg_savingStatus.color = if saveResultsToDisk then (if theUsage > 0 then (red*0.85) else color 100 255 100) else white*0.5
			updateMemoryCacheGraph()
			
		)		
		
		fn updateUI =
		(
			btn_createVDBLoader.enabled = saveResultsToDisk
			updateMemoryCount()
			updateAsyncControlsUI()
			getFreeSpaceOnDisk (getCachePath())
		)
		
		fn createWaitDialog =
		(
			rollout StokeFieldSim_SavingPleaseWait_Rollout "STOKE Saving, Please Wait..."
			(
				label lbl_01 "Saving Memory Cache to PRT Files, this can take a while..."
			)
			StokeFieldSim_SavingPleaseWait_Rollout
		)		
		
		/*
		fn flushCacheToCurrentPath =
		(
			local StokeFieldSim_SavingPleaseWait_Rollout = createWaitDialog()
			createDialog  StokeFieldSim_SavingPleaseWait_Rollout 400 25
			try(
				if saveResultsToDisk then
					this.delegate.FlushCache()
				else
				(
					local outPath = getCachePath()  + "StokeField_" +randomID+"\\"+ outputVersion + "\\" + outputPrefix+"_####.prt" 
					this.delegate.WriteCache outPath
				)
				destroyDialog StokeFieldSim_SavingPleaseWait_Rollout 
			)catch(
				destroyDialog StokeFieldSim_SavingPleaseWait_Rollout 
				messageBox ( getCurrentException() ) title:"StokeFieldSim Flush Error"
			)
			updateUI()
		)		
		*/
		
		fn setCachePath =
		(
			local outPath = getCachePath() + "StokeField_" +randomID+"\\" + outputVersion + "\\"
			makeDir outPath all:true
			if saveResultsToDisk then
			(
				/*
				if this.delegate.SequenceCacheUsageMB > 0 then
					q = querybox "Setting the Disk Cache Path will CLEAR the Memory Cache.\n\nClick [Yes] to CLEAR the Memory Cache\nand Set the Disk Cache Path.\n\nClick [No] to CANCEL and keep the current Memory Cache\nand keep the current Disk Cache Path" title:"Set Disk Cache Path - Clear Memory?"
				else 
					q = true
				
				if q then
				*/
				this.delegate.InitializeCache ( outPath+outputPrefix+"_####.vdb" )
			)
			else this.delegate.InitializeCache ""
			updateMemoryCount()
		)
		
		on chk_saveResultsToDisk changed state do 
		(
			setCachePath()
			updateUI()
		)
		on edt_outputVersion entered txt do setCachePath()
		on edt_outputPrefix entered txt do setCachePath()
		
		fn resetCache =
		(
			local doUpdate = false
			if saveResultsToDisk  then
			(
				if this.delegate.SequenceCacheUsageMB > 0 then
					local q = (querybox  "Are you sure you want to RESET the Cache?\n\nAny unsaved data will be lost!" title:"RESET StokeFieldSim Cache")
				else
					q = true
				if q do
				(
					this.delegate.ResetCache()
					gc light:true
					updateUI()
				)
			)
			else
			(
				if (querybox  "Are you sure you want to RESET the Memory Cache?\n\nDisk Cache Is Disabled!\nAny unsaved data will be lost!" title:"RESET StokeFieldSim Memory Cache") do 
				(
					this.delegate.ResetCache()
					updateUI()
				)
			)
		)
		
		fn saveCacheToPRTSequence fromMemory:false =
		(
			local theNewPath = getSaveFileName caption:"Save the StokeFieldSim Memory Cache to a New OpenVDB File Sequence" filename:"StokeField_" types:"OpenVFB (*.vdb)|*.vdb"
			if theNewPath != undefined do
			(
				local StokeFieldSim_SavingPleaseWait_Rollout = createWaitDialog()
				createDialog  StokeFieldSim_SavingPleaseWait_Rollout 400 25
				this.delegate.WriteCache theNewPath FromMemoryOnly:fromMemory
				destroyDialog StokeFieldSim_SavingPleaseWait_Rollout 
				updateUI()
			)
		)
		
		on btn_stopAsyncCache pressed do
		(
			isAsyncFlushCancelled = true
			if StokeGlobalInterface.LoggingLevel != #none do			
				format "--STOKE FIELD ASYNC DISK CACHE SAVING CANCELLED BY USER!\n"
			local st = timestamp()
			--this.delegate.ResetCache()
			this.delegate.CancelFlushCache()
			if StokeGlobalInterface.LoggingLevel == #debug do			
				format "--Resetting Cache Took % Seconds.\n" ((timestamp()-st)/1000.0)
			/*local st = timestamp()	
			local outPath = getCachePath() + "StokeField_" +randomID+"\\" + outputVersion + "\\"
			this.delegate.InitializeCache ( outPath+outputPrefix+"_####.prt" )
			if StokeGlobalInterface.LoggingLevel == #debug do			
				format "--Cache Path Set To [%] in % seconds.\n" ( outPath+outputPrefix+"_####.prt" ) ((timestamp()-st)/1000.0)*/
			updateUI()
		)
		

		
		fn createClearMemoryMenu =
		(
			rcmenu StokeFieldSim_ClearMemory_Menu 
			(
				menuItem mnu_resetCache "RESET the Memory Cache..."
				separator sep_10
				--menuItem mnu_flushCacheToCurrentPath "FLUSH the Memory Cache - Save to current Cache Path"
				--separator set_20
				menuItem mnu_saveCacheToPRTSequence "SAVE the Cache as a NEW VDB Sequence..."
				
				on mnu_resetCache picked do resetCache()
				--on mnu_flushCacheToCurrentPath picked do flushCacheToCurrentPath()
				on mnu_saveCacheToPRTSequence picked do saveCacheToPRTSequence fromMemory:false
			)
			StokeFieldSim_ClearMemory_Menu
		)		
		
		on btn_clearParticleCache pressed do
		(
			popupMenu (createClearMemoryMenu()) pos:mouse.screenpos
		)
		on btn_clearParticleCache rightclick do
		(
			popupMenu (createClearMemoryMenu()) pos:mouse.screenpos
		)		
		
		fn createVDBLoader =
		(
			local outPath = getCachePath() + "StokeField_" +randomID+"\\"+ outputVersion + "\\" 
			local loaderObject = StokeFieldLoader name:(uniquename ("VDB_"+selection[1].name + "_"))
			loaderObject.filenamePattern = ( outPath+outputPrefix+"_0000.vdb" )
			loaderObject.wirecolor = color 255 200 100		
			select loaderObject					
		)
		
		on btn_createVDBLoader pressed do
		(
			createVDBLoader()
		)		
		on btn_createVDBLoader rightclick do
		(
			createVDBLoader()
		)		
		
		on edt_outputPath entered txt do
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
			setCachePath()
			updateUI()	
		)
		
		fn pickOutputPath = 
		(
			theNewPath = getSavePath initialDir:(getCachePath())
			if theNewPath != undefined do outputPath = theNewPath
			if outputPath == "" do outputPath = "$default\\"
			updateUI()
		)
		
		fn exploreOutputPath =
		(
			local theCachePath = getCachePath()
			local outPath = theCachePath + "StokeField_" +randomID+"\\"+ outputVersion
			if doesFileExist outPath then
				shellLaunch "explorer.exe" outPath
			else
			(
				outPath =  theCachePath + "StokeField_" +randomID+"\\"
				if doesFileExist outPath then 
					shellLaunch "explorer.exe" outPath
				else
					shellLaunch "explorer.exe" theCachePath
			)
		)
		
		fn createOutputPathMenu =
		(
			rcmenu StokeFieldSim_OutputPath_Menu 
			(
				menuItem mnu_pickOutputPath "PICK Base Output Path..."
				separator sep_10
				menuItem mnu_setPathAsDefault "SAVE Current Base Output Path As USER Default..."
				menuItem mnu_getUserDefaultPath "RESET Base Output Path To USER Default"
				separator sep_20
				menuItem mnu_getDefaultPath "RESET Base Output Path To FACTORY $default"
				menuItem mnu_insertSceneToken "INSERT $scene Token"
				menuItem mnu_insertUserToken "INSERT $user Token"
				separator sep_30
				menuItem mnu_warnAboutOverwriting "WARN About Overwriting Disk Cache Files" checked:(::StokeWarnAboutOverwriting)
				separator sep_40
				menuItem mnu_expandPath "EXPAND Base Output Path"
				separator sep_50
				menuItem mnu_exploreOutputPath "EXPLORE Full Output Path..."
				
				on mnu_warnAboutOverwriting  picked do
				(
					StokeWarnAboutOverwriting = not StokeWarnAboutOverwriting
					setIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Options" "WarnAboutOverwriting" (StokeWarnAboutOverwriting as string)
				)				
				
				on mnu_pickOutputPath picked do pickOutputPath()
				on mnu_exploreOutputPath picked do exploreOutputPath()
				on mnu_setPathAsDefault picked do 
				(
					setIniSetting (getDir #plugcfg + "\\StokeFieldSimPreferences.ini") "Output" "Default" outputPath
				)
				on mnu_getUserDefaultPath picked do 
				(
					local theVal = getIniSetting (getDir #plugcfg + "\\StokeFieldSimPreferences.ini") "Output" "Default"
					if try(doesFileExist theVal)catch(false) then 
						outputPath = theVal
					else
						outputPath = "$default\\"
					updateUI()
				)
				on mnu_getDefaultPath picked do 
				(
					outputPath = "$default\\"
					updateUI()
				)
				on mnu_insertSceneToken picked do
				(
					outputPath += "$scene\\"
					updateUI()
				)
				on mnu_insertUserToken picked do
				(
					outputPath += "$user\\"
					updateUI()
				)				
				
				on mnu_expandPath picked do
				(
					outputPath = getCachePath()
					updateUI()
				)
				
			)
			StokeFieldSim_OutputPath_Menu
		)
		
		on btn_getOutputPath rightClick do
		(
			local StokeFieldSim_OutputPath_Menu = createOutputPathMenu()
			popupMenu StokeFieldSim_OutputPath_Menu pos:mouse.screenpos
		)	
		
		on btn_getOutputPath pressed do
		(
			local StokeFieldSim_OutputPath_Menu = createOutputPathMenu()
			popupMenu StokeFieldSim_OutputPath_Menu pos:mouse.screenpos			
		)		
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
		)		
		
		on btn_outputVersion pressed do 
		(
			createVersionRCMenu()
			popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
		)
		
		on btn_outputPrefix pressed do
		(
			if createPrefixRCMenu() do 
				popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
		)

		on btn_memoryCacheStep_Preset pressed do popupPresetMenu #memoryCacheStep
		on btn_memoryCacheStep_Preset rightclick do popupPresetMenu #memoryCacheStep
			
		on btn_memoryLimit_Preset pressed do popupPresetMenu #memoryLimit
		on btn_memoryLimit_Preset rightclick do popupPresetMenu #memoryLimit
			
		on btn_threadLimit_Preset pressed do popupPresetMenu #threadLimit
		on btn_threadLimit_Preset rightclick do popupPresetMenu #threadLimit
			
		fn updateLayout = 
		(
			edt_outputPath.width = cacheParamsRollout.width - 22
			edt_outputPath.pos.x = 2
			edt_memoryUsed.width = cacheParamsRollout.width - 34
			edt_memoryUsed.pos.x = 8
			
			edt_outputVersion.width = edt_outputPrefix.width = (cacheParamsRollout.width - 44)/2
			edt_outputVersion.pos.x = 2
			edt_outputPrefix.pos.x = edt_outputVersion.width + 24
			btn_outputVersion.pos.x = edt_outputVersion.width + 2
			btn_outputPrefix.pos.x = btn_getOutputPath.pos.x
			
			edt_driveinfo.width = cacheParamsRollout.width-4
			edt_driveinfo.pos.x = 2
			
			btn_stopAsyncCache.pos.x = chk_saveResultsToDisk.pos.x = cacheParamsRollout.width-144
			
			try(chk_saveResultsToDisk.width = cacheParamsRollout.width-45
				chk_saveResultsToDisk.pos.x = 20
			)catch()			
			
			spn_memoryCacheStep.pos.x = spn_threadLimit.pos.x = spn_memoryLimit.pos.x = cacheParamsRollout.width-38
			
			btm_memoryCacheMap.width = cacheParamsRollout.width-8
			btm_memoryCacheMap.pos.x = 4
		)		

		on cacheParamsRollout open do
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
		)
	)
	
	fn onMagmaError msg NodeID:unsupplied ExpressionID:unsupplied = 
	(
		-- TODO: Update the error status of the appropriate magma holder.
		if msg != undefined then
		(
			--if NodeID != unsupplied do format "NodeID: %\n" NodeID
			if ExprID != unsupplied do 
			(
				if ExpressionID == 0 do
				(
					format "StokeFieldSim INITIAL Flow error:\n%" msg
					simRollout.prg_initSimError.color = red
					
					local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == holder.initMagmaHolder collect i)
					if existingEditor.count == 1 do
					(
						existingEditor[1][2].updateErrorLog NodeID:NodeID ErrorMessage:msg
					)
					LastInitErrorID = nodeID
					LastInitErrorMsg = msg
				)
				if ExpressionID == 1 do
				(
					format "StokeFieldSim SIMULATION Flow error:\n%" msg
					simRollout.prg_updateSimError.color = red
					local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == holder.MagmaHolder collect i)
					if existingEditor.count == 1 do
					(
						existingEditor[1][2].updateErrorLog NodeID:NodeID ErrorMessage:msg
					)
					LastSimErrorID = nodeID
					LastSimErrorMsg = msg
				)				
			)
			--format "\n"
		)
		else
		(
			simRollout.prg_initSimError.color = green
			simRollout.prg_updateSimError.color = green
		)
	)
	
	rollout paramsRollout "Simulation"
	(
		label lbl_simoptions "" offset:[0,-24]
		group "Simulation Options"
		(
			dotNetControl btn_simulate "Button" text:"SIMULATE" width:84 height:30 align:#left offset:[-6,-4] across:2 tooltip:"Starts a new simulation from the 'Simulate From' frame.\n\nAny previous simulation data in memory or on disk will be replaced."
			dotNetControl btn_resume "Button" text:"RESUME" width:64 height:30 align:#right offset:[6,-4] tooltip:"Resumes a stopped simulation from the last processed frame.\n\nExisting simulation data in memory and on disk will be preserved."
			
			--progressbar prg_progress visible:false height:8 offset:[0,-36] color:green width:148 align:#center
			dotNetControl btn_cancel "Button" text:"STOP" width:148 height:30 align:#center offset:[-10000,-36] tooltip:"Press to stop the simulation.\n\nYou can resume it later using the RESUME button."
			
			spinner spn_startTime "Simulate From  " range:[-100000,1000000,0]  type:#integer fieldwidth:38 offset:[56,-3] across:2 align:#right tooltip:"Defines the first frame of the simulation."
			button btn_startTime_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the first frame of the simulation."
			
			checkbox chk_useCacheStartTime "Cache From" offset:[-5,-4] across:3 align:#left tooltip:"When checked, the simulation results will be cached only after the specified frame."
			spinner spn_cacheStartTime "" type:#integer range:[-100000,1000000,0] fieldwidth:38 offset:[34,-3] align:#right tooltip:"Specifies the start frame of the cache.\n\nThis can be used to perform a pre-roll unitl the specified frame without caching data, then cache/save the particles after this frame."
			button btn_cacheStartTime_Preset ">" width:18 height:16 align:#right offset:[7,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set how many frames to skip before updating the viewport during Simulation."
			
			spinner spn_endTime "Simulate Until  " range:[-100000,1000000,0]  type:#integer fieldwidth:38 offset:[56,-3] across:2 align:#right tooltip:"Defines the last frame of the simulation."
			button btn_endTime_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the last frame of the simulation."

			spinner spn_subSteps "Sub-Steps  " range:[1,100,1] type:#integer fieldwidth:38 offset:[56,-3] across:2 align:#right tooltip:"Defines the number of simulation sub-steps per frame."
			button btn_subSteps_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the number of simulation sub-steps per frame."
			
			checkbox chk_updateViews "Preview Nth" offset:[-5,-4] across:3 align:#left tooltip:"When checked, the simulation results will be displayed in the viewport to preview the process.\n\nUse the value to the right to specify how often to refresh the viewport."
			spinner spn_updateViewsEvery "" type:#integer range:[1,100,1] fieldwidth:38 offset:[34,-3] align:#right tooltip:"Specifies how many frames to skip before updating the viewport during Simulation."
			button btn_updateViewsEvery_Preset ">" width:18 height:16 align:#right offset:[7,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set how many frames to skip before updating the viewport during Simulation."
		)
		
		dropdownlist ddl_usesolver items:#("No Fluid Solver","Simple Fluid Solver") width:154 align:#center offset:[0,-3]
		
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
		)
		
		
		on btn_updateViewsEvery_Preset pressed do popupPresetMenu #updateViewsEvery
		on btn_updateViewsEvery_Preset rightclick do popupPresetMenu #updateViewsEvery
		on btn_startTime_Preset pressed do popupPresetMenu #startTime
		on btn_startTime_Preset rightclick do popupPresetMenu #startTime
		on btn_endTime_Preset pressed do popupPresetMenu #endTime
		on btn_endTime_Preset rightclick do popupPresetMenu #endTime
		on btn_subSteps_Preset pressed do popupPresetMenu #subSteps
		on btn_subSteps_Preset rightclick do popupPresetMenu #subSteps
		on btn_cacheStartTime_Preset pressed do popupPresetMenu #cacheStartTime
		on btn_cacheStartTime_Preset rightclick do popupPresetMenu #cacheStartTime
		
		
		
		on btn_emitEndTime_Preset pressed do popupPresetMenu #emitEndTime
		on btn_emitEndTime_Preset rightclick do popupPresetMenu #emitEndTime
		on btn_Rate_Preset pressed do popupPresetMenu #rate
		on btn_Rate_Preset rightclick do popupPresetMenu #rate
			
		fn updateRateTooltip =
		(
		)
		
		fn updateSimAndResumeState =
		(
			btn_simulate.enabled = true
			btn_resume.enabled = theSim != undefined and theSim.GetCurrentTime() != undefined and theSim.GetCurrentTime() < endTime and getHashSum() == lastSimHash
		)
		
		fn updateUI =
		(
			updateSimAndResumeState()
			cacheParamsRollout.updateMemoryCacheGraph()
			
			spn_cacheStartTime.enabled = btn_cacheStartTime_Preset.enabled = useCacheStartTime
			spn_updateViewsEvery.enabled = btn_updateViewsEvery_Preset.enabled = updateViews
		)	
		
		on chk_useCacheStartTime changed state do updateUI()
		on chk_updateViews changed state do updateUI()
		on spn_subSteps changed val do updateUI()
		
		on spn_StartTime changed val do updateUI()
		on spn_EndTime changed val do updateUI()
		

		fn simulateFunction mode:#sim =
		(
			theMaxVersion = (maxVersion())[1] 

			local saveThreads = sysinfo.cpucount/2
			if saveResultsToDisk do
			(
				if saveThreads > threadLimit and threadLimit != 0 do saveThreads = threadLimit 
				try(this.delegate.SetNumSerializerThreads saveThreads)catch()
				if StokeGlobalInterface.LoggingLevel != #none do format "--Stoke Field Asynchronous Saving During Simulation Using % Threads On % Cores.\n" saveThreads sysinfo.cpucount
			)
			
			--Update Error States:
			LastSimErrorID = -100
			LastSimErrorMsg = undefined
			LastInitErrorID = -100
			LastInitErrorMsg = undefined
			simRollout.prg_updateSimError.color = simRollout.prg_initSimError.color = green
			local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == holder.initMagmaHolder collect i)
			if existingEditor.count == 1 do existingEditor[1][2].updateErrorLog NodeID:-1 ErrorMessage:undefined 
			local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == holder.MagmaHolder collect i)
			if existingEditor.count == 1 do existingEditor[1][2].updateErrorLog NodeID:-1 ErrorMessage:undefined 
			
			if mode == #sim then
			(
				lastSimHash = getHashSum()
				
				--theSim.SetInitExpression ( SimEmberInitFunctor expressionHolder:(this.holder) )
				--theSim.SetUpdateExpression ( SimEmberUpdateFunctor expressionHolder:(this.holder) )
				--theSim.SetBounds [boundsMinX,boundsMinY,boundsMinZ] [boundsMaxX, boundsMaxY,boundsMaxZ]
				--theSim.SetSpacing gridSpacing
				
				theSim = SimEmberFactory.CreateFieldCollection()
				theSim.SetSimulationRange startTime endTime numSubSteps:subSteps
				theSim.SetInitMagmaExpression this.holder.initMagmaHolder
				theSim.SetUpdateMagmaExpression this.holder.magmaHolder
				theSim.SetGridParameters [boundsMinX,boundsMinY,boundsMinZ] [boundsMaxX, boundsMaxY,boundsMaxZ] gridSpacing
				theSim.SetSolverEnabled (usesolver>1)
				theSim.SetErrorCallback this.onMagmaError
				
				this.delegate.ResetCache()
				if saveResultsToDisk then
				(
					local outPath = getCachePath() + "StokeField_" +randomID+"\\" + outputVersion + "\\"
					
					this.delegate.SequencePath = ( outPath+outputPrefix+"_####.vdb" )
					this.delegate.UseDiskCache = true
					
					-- I'd prefer not to delete the files (since its 1. a waste of time, and 2. a leaky abstraction about the storage algorithm of the disk cache)
					-- In the future let's have a callback that trims any files that aren't part of the cache to achieve this same effect.
					
					local theFilesToDelete = getFiles (outPath+outputPrefix+"_*.vdb")
					
					if theFilesToDelete.count > 0 and ::StokeWarnAboutOverwriting != false do
					(
						local theNewVersion = getNewVersionFolder()
						local q = yesnocancelbox ("Existing SIMULATION DATA FOUND in the Output Folder.\nAre you SURE you want to overwrite existing files?\n\nClick [Yes] to OVERWRITE.\nClick [No] to INCREMENT the Cache Version to "+theNewVersion as string+".\nClick [Cancel] to CANCEL the Simulation.\n\nYou can disable this message in\nthe Output Path Menu (folder icon).") title:"Overwrite Disk Cache?"
						case q of
						(
							#yes: () --do nothing
							#no: (
								createNewVersion theNewVersion
								outPath = getCachePath() + "StokeField_" +randomID+"\\" + outputVersion + "\\"	
							) --increment the cache!
							#cancel: return false
						)
					)					
					
					
					/*
					local q = if pathIsNetworkPath outPath and theFilesToDelete.count > 0 then
						querybox ("An existing Disk Cache VDB Sequence with "+theFilesToDelete.count as string+" files exists\nin the output folder, and you are writing to a Network location.\n\nClick [Yes] to DELETE the existing files first.\nDepending on the network speed, this operation could be slow.\n\nIf you answer [No] and later cancel the simulation,\nyou might end up with files from different simulations.\n") title:"Delete Existing Cache From Network?"
					else
						true
					
					if q do
					(
						local st = timestamp()
						if StokeGlobalInterface.LoggingLevel != #none do			
							format "--Deleting % Old Cache Files...\n" theFilesToDelete.count
						
						for f in theFilesToDelete do 
						(
							deleteFile f
							if StokeGlobalInterface.LoggingLevel == #debug do format "--Deleted Old File [%].\n" f
							if theMaxVersion > 12000 do windows.processPostedMessages()
						)
						if StokeGlobalInterface.LoggingLevel != #none do			
							format "--Deleting Old Cache Took % Seconds.\n" ((timestamp()-st)/1000.0)
					)
					*/
				)else(
					this.delegate.UseDiskCache = false
				)
			)
			else
			(
				theSim.SetSimulationRange (theSim.getCurrentTime()) endTime numSubSteps:subSteps
			)
			
			if saveResultsToDisk then
			(
				local theMemoryLimit = memoryLimit
				local theFixedBufferSize = 512
				if memoryLimit < 1024 do
				(
					theMemoryLimit = 1024
					theFixedBufferSize = 128
				)
				this.delegate.SequenceCacheCapacityMB = theFixedBufferSize
				this.delegate.SerializeQueueCapacityMB = theMemoryLimit - theFixedBufferSize
			)
			else
			(
				this.delegate.SequenceCacheCapacityMB = memoryLimit
				this.delegate.SerializeQueueCapacityMB =  0
			)

			if theMaxVersion < 13000 do progressStart "Simulating..."
			escapeEnable = false
			isSimulating = true
			StokeFieldSim_Progress_Cancel = false
			--btn_resume.visible = btn_simulate.visible = false
			btn_resume.pos.x = btn_simulate.pos.x = -10000 
			btn_cancel.pos.x = 8
--			prg_progress.visible = true
			
			-- NB! Must call EndRenderMode with same list as BeginRenderMode
			--local activeDistSources = for i = 1 to DistSources.count where isValidNode DistSources[i] and DistSourcesOnOff[i] != False collect DistSources[i]
			--if not useViewportParticles do 
			--	StokeGlobalInterface.BeginRenderMode activeDistSources
			
			local st = timestamp()
			
			local theCallback = StokeFieldSimCallback StokeFieldSimPlugin:this StokeFieldSim:theSim
			local result = #success
			local errorMessage = ""
			
			try(
				result = theSim.Simulate frameCallback:(theCallback.onFrameUpdate)
			)catch(
				result = #error
				errorMessage = getCurrentException()
			)
			
			-- NB! Must call EndRenderMode with same list as BeginRenderMode
			--if not useViewportParticles do 
			--	StokeGlobalInterface.EndRenderMode activeDistSources

			if theMaxVersion < 13000 do progressEnd()
			
			--btn_resume.visible = btn_simulate.visible = true
			--btn_cancel.visible = false
			btn_resume.pos.x = 8+ btn_resume.width+1
			btn_simulate.pos.x = 8 
			btn_cancel.pos.x = -10000
			
			--prg_progress.visible = false
			

			if this.delegate.ViewportEnabled do
			(
				this.delegate.ViewportEnabled = false
				this.delegate.ViewportEnabled = true
			)
			
			cacheParamsRollout.updateUI()
			updateUI()
			escapeEnable = true
			
			
			local totalTime = timestamp()-st
			local theTimeString = ("StokeFieldSim Time: "+(totalTime/1000.0) as string +" sec." )
			pushPrompt theTimeString 
			if StokeGlobalInterface.LoggingLevel != #none do
			(
				format "--Update Time: % sec.\n" (theSim.totalUpdateTime /1000.0)
				format "--Advect Time: % sec.\n" (theSim.totalAdvectTime /1000.0)
				format "--Discretize Time: % sec.\n" (theSim.totalDiscretizeTime /1000.0)
				format "--Solver Time: % sec.\n" (theSim.totalSolverTime /1000.0)
				format "--Overhead Time: % sec.\n" ((totalTime-theSim.totalUpdateTime-theSim.totalAdvectTime-theSim.totalDiscretizeTime-theSim.totalSolverTime) /1000.0)
				format "--TOTAL %\n" theTimeString 
			)		
--			if updateViews do sliderTime = endTime

			if saveResultsToDisk do
			(
				saveThreads = sysinfo.cpucount-1
				if saveThreads < 1 do saveThreads  = 1 --anyone using one CPU?
				if saveThreads > threadLimit and threadLimit != 0 do saveThreads = threadLimit 
				if StokeGlobalInterface.LoggingLevel != #none do format "--Flushing Cache Using % Threads On % Cores.\n" saveThreads sysinfo.cpucount
				try(this.delegate.SetNumSerializerThreads saveThreads)catch()
				DoSetFlushFlag true
				isAsyncFlushCancelled = false
				theCallback.asyncCacheStart = timestamp()
				this.delegate.FlushCacheAsync theCallback.FlushReadyCallback
				this.delegate.SetSerializerCallback theCallback.FlushFrameReadyCallback
				append ::StokeFieldSimulationShutdownCallbacksArray theCallback
			)
			
			paramsDisplayRollout.updateUI()
			
			if result == #error and not IsNetServer() do (
				messageBox errorMessage title:"StokeFieldSim Simulation Error"
			)
			isSimulating = false
			return result			
		)

		on btn_simulate click arg do
		(
			simulateFunction()
		)
		
		on btn_resume click arg do
		(
			simulateFunction mode:#resume
		)		
		
		on btn_cancel click arg do
		(
			StokeFieldSim_Progress_Cancel = true
		)
		
		fn updateLayout =
		(
			btn_simulate.FlatStyle = btn_resume.FlatStyle = btn_cancel.FlatStyle = btn_simulate.FlatStyle.System
			btn_resume.width = btn_simulate.width = (paramsRollout.width-14)/2
			
			btn_simulate.pos.x = if isSimulating then -10000 else 8
			btn_resume.pos.x = if isSimulating then -10000 else btn_simulate.width+8
			btn_cancel.width = (paramsRollout.width-14)
			btn_cancel.pos.x = if isSimulating then 8 else -10000
				
			spn_startTime.pos.x = spn_cacheStartTime.pos.x = spn_endTime.pos.x = spn_subSteps.pos.x =  spn_updateViewsEvery.pos.x = (paramsRollout.width-38)
			ddl_usesolver.width = (paramsRollout.width-8)
			ddl_usesolver.pos.x = 4
		)		
		
		on paramsRollout open do 
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
			onMagmaError LastInitErrorMsg NodeID:LastInitErrorID ExpressionID:0
			onMagmaError LastSimErrorMsg NodeID:LastSimErrorID ExpressionID:1			
		)
	)
	
	
	local dn_distribution_bg, dn_channels_bg, dn_velocity_bg, dn_velocity_fg, prtColor, surfaceColor, volumeColor, vertColor, edgeColor, grayColor, pflowColor, EmberColor, forceColor, fumeColor
	local VectorColor, FloatColor, IntColor
	
	fn getUIColors =
	(
		local textColor = ( ((colorman.getcolor #text) as color)*255)
		local maxBgColor = (((colorman.getcolor #window)) as color)*255
		
		local blackColor = (dotNetClass "System.Drawing.Color").fromARGB textColor.r textColor.g textColor.b
		if maxBgColor.v >= 160 then
		(
			dn_distribution_bg = (dotNetClass "System.Drawing.Color").fromARGB 240 235 230 
			dn_channels_bg = (dotNetClass "System.Drawing.Color").fromARGB 220 230 220
			
			dn_velocity_bg = (dotNetClass "System.Drawing.Color").fromARGB 220 220 230 
			dn_velocity_fg  = (dotNetClass "System.Drawing.Color").fromARGB 0 0 0
		
			prtColor =  (dotNetClass "System.Drawing.Color").fromARGB 0 0 200
			surfaceColor =  (dotNetClass "System.Drawing.Color").fromARGB 0 100 0
			volumeColor =  (dotNetClass "System.Drawing.Color").fromARGB 100 0 200 
			vertColor = (dotNetClass "System.Drawing.Color").fromARGB 0 100 150 
			edgeColor = (dotNetClass "System.Drawing.Color").fromARGB 200 0 200 
			grayColor = (dotNetClass "System.Drawing.Color").fromARGB 180 180 180
			pflowColor =  (dotNetClass "System.Drawing.Color").fromARGB 150 0 100		
			fumeColor =  (dotNetClass "System.Drawing.Color").fromARGB 200 100 0
			EmberColor = (dotNetClass "System.Drawing.Color").fromARGB 150 0 0		
			forceColor = (dotNetClass "System.Drawing.Color").fromARGB 0 100 0

			VectorColor = (dotNetClass "System.Drawing.Color").fromARGB 0 0 0
			FloatColor = (dotNetClass "System.Drawing.Color").fromARGB 0 100 0
			IntColor = (dotNetClass "System.Drawing.Color").fromARGB 0 0 150				
		)
		else
		(
			dn_distribution_bg = (dotNetClass "System.Drawing.Color").fromARGB 80 70 70 
			dn_channels_bg = (dotNetClass "System.Drawing.Color").fromARGB 70 80 70
			
			dn_velocity_bg = (dotNetClass "System.Drawing.Color").fromARGB 70 70 80 
			dn_velocity_fg  = (dotNetClass "System.Drawing.Color").fromARGB 255 255 255			

			prtColor =  (dotNetClass "System.Drawing.Color").fromARGB 100 100 255
			surfaceColor =  (dotNetClass "System.Drawing.Color").fromARGB 200 255 200
			volumeColor =  (dotNetClass "System.Drawing.Color").fromARGB 220 200 255 
			vertColor = (dotNetClass "System.Drawing.Color").fromARGB 150 220 255 
			edgeColor = (dotNetClass "System.Drawing.Color").fromARGB 255 200 255 
			grayColor = (dotNetClass "System.Drawing.Color").fromARGB 200 200 200
			pflowColor =  (dotNetClass "System.Drawing.Color").fromARGB 255 100 200 
			fumeColor =  (dotNetClass "System.Drawing.Color").fromARGB 255 200 100 
			EmberColor =  (dotNetClass "System.Drawing.Color").fromARGB 255 100 100	
			forceColor = (dotNetClass "System.Drawing.Color").fromARGB 200 255 200

			VectorColor = (dotNetClass "System.Drawing.Color").fromARGB 255 255 255
			FloatColor = (dotNetClass "System.Drawing.Color").fromARGB 200 255 200
			IntColor = (dotNetClass "System.Drawing.Color").fromARGB 200 200 255					
		)			
	)	
	
	rollout paramsTimingRollout "Playback Retiming"
	(
		checkbox chk_UsePlaybackTime "Use Graph" across:3 offset:[-8,-3]
		spinner spn_PlaybackTime "" range:[-1000000,1000000,0] type:#float fieldwidth:50 offset:[38,-2] controller:this.delegate.Playbacktime.controller
		button btn_PlaybackTime_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the value of the Playback Graph spinner."
		
		--checkbox chk_UsePlaybackInterpolation "Use Playback Interpolation" offset:[-8,-3]
		
		on chk_UsePlaybackTime changed val do this.delegate.UsePlaybackTime = val
		on spn_PlaybackTime changed val do this.delegate.PlaybackTime = val
			
		fn playbackgraph_currentSegment mode =
		(
			try(deleteKeys this.delegate.PlaybackTime.controller #allKeys)catch()
			local theCtrl = this.delegate.PlaybackTime.controller = bezier_float()
			case mode of
			(
				#linear:
				(
					theKey = addNewKey theCtrl animationrange.start
					theKey.value = animationrange.start.frame
					theKey.inTangentType = theKey.outTangentType = #linear
					
					theKey = addNewKey theCtrl animationrange.end
					theKey.value = animationrange.end.frame
					theKey.inTangentType = theKey.outTangentType = #linear
					
					setBeforeORT theCtrl #linear
					setAfterORT theCtrl #linear
				)	
				#accel:
				(
					theKey = addNewKey theCtrl animationrange.start
					theKey.value = animationrange.start.frame
					theKey.inTangentType = theKey.outTangentType = #slow
					
					theKey = addNewKey theCtrl animationrange.end
					theKey.value = animationrange.end.frame
					theKey.inTangentType = theKey.outTangentType = #fast
					
					setBeforeORT theCtrl #linear
					setAfterORT theCtrl #linear
				)
				#decel:
				(
					theKey = addNewKey theCtrl animationrange.start
					theKey.value = animationrange.start.frame
					theKey.inTangentType = theKey.outTangentType = #fast
					
					theKey = addNewKey theCtrl animationrange.end
					theKey.value = animationrange.end.frame
					theKey.inTangentType = theKey.outTangentType = #slow
					
					setBeforeORT theCtrl #linear
					setAfterORT theCtrl #linear
				)		

				#pingpong:
				(
					theKey = addNewKey theCtrl animationrange.start
					theKey.value = animationrange.start.frame
					theKey.inTangentType = theKey.outTangentType = #auto
					
					theKey = addNewKey theCtrl (animationrange.start.frame + ((animationrange.end.frame - animationrange.start.frame)/2))
					theKey.value = animationrange.end.frame
					theKey.inTangentType = theKey.outTangentType = #auto
					
					theKey = addNewKey theCtrl animationrange.end
					theKey.value = animationrange.start.frame
					theKey.inTangentType = theKey.outTangentType = #auto
					
					setBeforeORT theCtrl #linear
					setAfterORT theCtrl #linear
				)	
			)
		)
		
		fn playbackgraph_invertAnimation =
		(
			local theCtrl = try(this.delegate.PlaybackTime.controller)catch(undefined)
			if theCtrl != undefined do
			(
				try(reverseTime theCtrl theCtrl.keys[1].time theCtrl.keys[theCtrl.keys.count].time  #incLeft #incRight)catch()
			)	
		)	
		
		fn playbackgraph_deleteAnimation = 
		(
			local theCtrl = try(this.delegate.PlaybackTime.controller)catch(undefined)
			if theCtrl != undefined do
				try(deleteKeys theCtrl #allKeys)catch()
		)		
			
		fn openPlaybackMenu =
		(
			rcmenu StokeFieldSim_PlaybackTime_Presets_RCMenu 
			(
				menuitem mnu_playbackgraph_currentSegment_Linear "Create LINEAR Playback Keys"
				menuitem mnu_playbackgraph_currentSegment_Accelerate "Create ACCELERATION Playback Keys"
				menuitem mnu_playbackgraph_currentSegment_Decelerate "Create DECELERATION Playback Keys"
				menuitem mnu_playbackgraph_currentSegment_PingPong "Create PING-PONG Playback Keys"
				separator sep_10 
				subMenu "Out-Of-Range Types"
				(
					menuitem mnu_playbackgraph_ORT_Before_Constant "Before CONSTANT" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #constant)catch(false))
					menuitem mnu_playbackgraph_ORT_Before_Cycle "Before CYCLE" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #cycle)catch(false))
					menuitem mnu_playbackgraph_ORT_Before_Loop "Before LOOP" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #loop)catch(false))
					menuitem mnu_playbackgraph_ORT_Before_PingPong "Before PING PONG" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #pingpong)catch(false))
					menuitem mnu_playbackgraph_ORT_Before_Linear "Before LINEAR" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #linear)catch(false))
					menuitem mnu_playbackgraph_ORT_Before_Repeat "Before RELATIVE REPEAT" checked:(try(getBeforeORT this.delegate.PlaybackTime.controller == #relativerepeat)catch(false))
					separator sep_100
					menuitem mnu_playbackgraph_ORT_After_Constant "After CONSTANT" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #constant)catch(false))
					menuitem mnu_playbackgraph_ORT_After_Cycle "After CYCLE" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #cycle)catch(false))
					menuitem mnu_playbackgraph_ORT_After_Loop "After LOOP" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #loop)catch(false))
					menuitem mnu_playbackgraph_ORT_After_PingPong "After PING PONG" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #pingpong)catch(false))
					menuitem mnu_playbackgraph_ORT_After_Linear "After LINEAR" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #linear)catch(false))
					menuitem mnu_playbackgraph_ORT_After_Repeat "After RELATIVE REPEAT" checked:(try(getAfterORT this.delegate.PlaybackTime.controller == #relativerepeat)catch(false))
				)
				separator sep_20 
				menuitem mnu_playbackgraph_invertAnimation "INVERT Existing Animation"
				separator sep_30 
				menuitem mnu_playbackgraph_deleteAnimation "DELETE Existing Animation"
				
				on mnu_playbackgraph_ORT_Before_Constant picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #constant)catch()
				on mnu_playbackgraph_ORT_Before_Cycle picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #cycle)catch()
				on mnu_playbackgraph_ORT_Before_Loop picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #loop)catch()
				on mnu_playbackgraph_ORT_Before_PingPong picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #pingpong)catch()
				on mnu_playbackgraph_ORT_Before_Linear picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #linear)catch()
				on mnu_playbackgraph_ORT_Before_Repeat picked do
					try(setBeforeORT this.delegate.PlaybackTime.controller #relativerepeat)catch()
				
				on mnu_playbackgraph_ORT_After_Constant picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #constant)catch()
				on mnu_playbackgraph_ORT_After_Cycle picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #cycle)catch()
				on mnu_playbackgraph_ORT_After_Loop picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #loop)catch()
				on mnu_playbackgraph_ORT_After_PingPong picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #pingpong)catch()
				on mnu_playbackgraph_ORT_After_Linear picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #linear)catch()
				on mnu_playbackgraph_ORT_After_Repeat picked do
					try(setAfterORT this.delegate.PlaybackTime.controller #relativerepeat)catch()				
				
				on mnu_playbackgraph_currentSegment_Linear picked do 
				(
					playbackgraph_currentSegment #linear
				)
				on mnu_playbackgraph_currentSegment_Accelerate picked do
				(
					playbackgraph_currentSegment #accel
				)
				on mnu_playbackgraph_currentSegment_Decelerate picked do
				(
					playbackgraph_currentSegment #decel
				)
				on mnu_playbackgraph_currentSegment_PingPong picked do 
				(
					playbackgraph_currentSegment #pingpong
				)
				
				on mnu_playbackgraph_invertAnimation picked do
				(					
					playbackgraph_invertAnimation()
				)
				on mnu_playbackgraph_deleteAnimation picked do 
				(
					playbackgraph_deleteAnimation()
				)
			)		
			popUpMenu StokeFieldSim_PlaybackTime_Presets_RCMenu position:mouse.screenPos	
		)
			
		on btn_PlaybackTime_Preset pressed do openPlaybackMenu()
		on btn_PlaybackTime_Preset rightclick do openPlaybackMenu()
		
		--on chk_UsePlaybackInterpolation changed val do this.delegate.UsePlaybackInterpolation = val

		fn updateLayout = 
		(
			spn_PlaybackTime.pos.x = paramsTimingRollout.width-34
		)		
		on paramsTimingRollout open do
		(
			enableAutoLayout()
			updateLayout()
			chk_UsePlaybackTime.state = this.delegate.UsePlaybackTime
			--chk_UsePlaybackInterpolation.state = this.delegate.UsePlaybackInterpolation
		)
	)	
	

	rollout paramsDisplayRollout "Viewport Display"
	(
		checkbox chk_displayInViewport "On" offset:[-10,-3] across:3

		spinner spn_viewportReduce "Reduce:" range:[0,999,delegate.ViewportReduce] type:#integer fieldwidth:43 offset:[37,-2]
		button btn_viewportReduce_Preset ">" width:18 height:16 align:#right offset:[10,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Request the reduction of the display samples by showing every Nth.\n\nNote that the system might reduce automatically beyond the requested value to maintain usability."
		
		dotNetControl btn_updateChannels "Button" text:"Update Channels" width:114 height:20 align:#right offset:[11,0] tooltip:"Updates the content of the Mask, Color and Display drop-down lists."
		
		label lbl_maskSource "Mask:" across:2 align:#left offset:[2,-1] tooltip:"Set the Mask Source"
		dropdownlist ddl_maskSource items:#("No Mask","Density") align:#right width:114 offset:[11,-3]
		
		label lbl_colorSource "Color:" across:2 align:#left offset:[2,1] tooltip:"Set the Color Source"
		dropdownlist ddl_colorSource items:#("Object Color","Color","Velocity") align:#right width:114 offset:[11,-3]

		label lbl_displayVectorSource "Display:" across:2 align:#left offset:[-7,1]
		dropdownlist ddl_displayVectorSource items:#("Large Dots","Velocity") align:#right width:114 offset:[11,-3]
		
		checkbox chk_ViewVectorNormalize "Norm.Length" offset:[-10,-2] across:3 tooltip:"Normalize the Vector channel before displaying it."
		spinner spn_viewVectorScale "" range:[0,1000,1.0] fieldwidth:42 offset:[38,-2] tooltip:"Scale the Vector channel before displaying it."
		button btn_viewVectorScale_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Scale the Vector channel before displaying it."
		
		checkbox chk_ViewportBoundsEnabled "Display Grid Bounds" offset:[-10,2]
		
		spinner spn_iconSize "Icon Size " range:[0.0,10000.0,30.0] fieldwidth:42 across:2 offset:[60,0] tooltip:"Set the size of the STOKE Viewport Icon." type:#worldunits
		button btn_iconSize_Preset ">" width:18 height:16 align:#right offset:[10,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the STOKE Viewport Icon."
		
		group "Manipulator Handles"
		(
			spinner spn_minManipulatorSize "Min. Size " range:[0.0,10000.0,0.1] fieldwidth:43 across:2 offset:[58,0] tooltip:"Set the minimum size of the STOKE Grid Manipulator Handles." type:#worldunits
			button btn_minManipulatorSize_Preset ">" width:18 height:16 align:#right offset:[8,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the minimum size of the STOKE Grid Manipulator Handles."
			spinner spn_maxManipulatorSize "Max. Size " range:[0.1,10000.0,10.0] fieldwidth:43 across:2 offset:[58,0] tooltip:"Set the maximum size of the STOKE Grid Manipulator Handles." type:#worldunits
			button btn_maxManipulatorSize_Preset ">" width:18 height:16 align:#right offset:[8,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the maximum size of the STOKE Grid Manipulator Handles."
		)
		
		spinner spn_forceUIUpdates "" range:[-100000,1000000,0.0] fieldwidth:42 across:2 offset:[0,-22] visible:false
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do 
				popUpMenu StokeFieldSim_Presets_RCMenu position:mouse.screenPos		
		)			
		
		on btn_minManipulatorSize_Preset pressed do popupPresetMenu #minManipulatorSize
		on btn_minManipulatorSize_Preset rightclick do popupPresetMenu #minManipulatorSize

		on btn_maxManipulatorSize_Preset pressed do popupPresetMenu #maxManipulatorSize
		on btn_maxManipulatorSize_Preset rightclick do popupPresetMenu #maxManipulatorSize		
		
		on btn_viewportReduce_Preset pressed do popupPresetMenu #viewportreduce
		on btn_viewportReduce_Preset rightclick do popupPresetMenu #viewportreduce		
			
		on btn_iconSize_Preset pressed do popupPresetMenu #iconSize
		on btn_iconSize_Preset rightclick do popupPresetMenu #iconSize
			
		on btn_viewVectorScale_Preset pressed do  popupPresetMenu #viewVectorScale
		on btn_viewVectorScale_Preset rightclick do  popupPresetMenu #viewVectorScale
			
		on ddl_maskSource selected itm do 
		(
			this.delegate.ViewportMaskChannel = if itm == 1 then "" else ddl_maskSource.selected
			redrawViews()
		)
		on ddl_colorSource selected itm do 
		(
			this.delegate.ViewportColorChannel = if itm == 1 then "" else ddl_colorSource.selected
			redrawViews()
		)
		on ddl_displayVectorSource selected itm do 
		(
			this.delegate.ViewportVectorChannel = if itm == 1 then "" else ddl_displayVectorSource.selected
			redrawViews()
		)		
			
		on chk_displayInViewport changed state do 
		(
			this.delegate.ViewportEnabled = state
			redrawViews()
		)
		
		on spn_viewportReduce changed val do 
		(
			delegate.ViewportReduce = val
			redrawViews()	
		)
		
		fn updateUI =
		(
			local theMagma = this.Holder.initMagmaHolder
			--local theChannels = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*" collect theMagma.getNodeProperty i "channelName" 
			local theVectors  = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*[3]"  collect theMagma.getNodeProperty i "channelName" 
			local theScalars = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and not matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*[3]"  collect theMagma.getNodeProperty i "channelName" 
			
			local maskSources = #("No Mask")
			for i in theScalars do appendIfUnique maskSources i
			ddl_maskSource.items = maskSources
			
			local colorSources = #("Object Color")
			for i in theVectors do appendIfUnique colorSources i
			for i in theScalars do appendIfUnique colorSources i
			ddl_colorSource.items = colorSources
			
			local displayVectors = #("Large Dots")
			for i in theVectors do appendIfUnique displayVectors i
			ddl_displayVectorSource.items = displayVectors
			
			spn_viewportReduce.value = delegate.ViewportReduce
			chk_displayInViewport.checked = this.delegate.ViewportEnabled
			
			local theIndex = findItem ddl_MaskSource.items this.delegate.ViewportMaskChannel
			if theIndex == 0 do theIndex = 1
			ddl_MaskSource.selection = theIndex
			
			local theIndex = findItem ddl_ColorSource.items this.delegate.ViewportColorChannel
			if theIndex == 0 do theIndex = 1
			ddl_ColorSource.selection = theIndex
			
			local theIndex = findItem ddl_displayVectorSource.items this.delegate.ViewportVectorChannel
			if theIndex == 0 do theIndex = 1
			ddl_displayVectorSource.selection = theIndex
			
			chk_ViewportBoundsEnabled.state = delegate.ViewportBoundsEnabled
		)
		
		on chk_ViewportBoundsEnabled  changed state do 
		(
			delegate.ViewportBoundsEnabled = state
		)			
		
		on btn_updateChannels click arg do updateUI()
		
		fn updateLayout = 
		(
			btn_updateChannels.FlatStyle = btn_updateChannels.FlatStyle.System
			
			btn_updateChannels.width = ddl_maskSource.width = ddl_colorSource.width = ddl_displayVectorSource.width = paramsDisplayRollout.width-53
			btn_updateChannels.pos.x = ddl_maskSource.pos.x= ddl_colorSource.pos.x = ddl_displayVectorSource.pos.x = 50
			
			spn_viewportReduce.pos.x = spn_viewVectorScale.pos.x = spn_iconSize.pos.x = paramsDisplayRollout.width-34
			
			chk_ViewVectorNormalize.text = if paramsDisplayRollout.width > 200 then "Normalize Length" else "Norm.Length"
			chk_ViewVectorNormalize.pos.x = 2
			
			spn_minManipulatorSize.pos.x = spn_maxManipulatorSize.pos.x = paramsDisplayRollout.width-36
			
		)		
		
		on paramsDisplayRollout open do
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
		)
	)	
	
	tool create 
	( 
		local theObj, startpoint, endpoint 
		on mousePoint clickno do
		(
			case clickno of
			(
				1:
				(
					nodeTM.translation = startpoint = gridPoint
					BoundsMinX = gridPoint.x
					BoundsMinY = gridPoint.y
					BoundsMinZ = gridPoint.z	
					pushprompt "HOLD DOWN Left Mouse Button and MOVE the Mouse to DEFINE Grid Base Extents. RELEASE LMB to DEFINE the Grid Height..."
				)
				2:
				(
					endpoint = gridPoint
					popprompt()
					pushprompt "LEFT-CLICK to Set the HEIGHT and FINISH Grid creation. RIGHT-CLICK to CANCEL creation..."
				)
				3: 
				(
					popprompt()
					pushprompt "LEFT-CLICK again to CREATE ANOTHER Field Sim object. RIGHT-CLICK to EXIT Field Sim creation mode."
					#stop
				)
			)
		)
		on mouseMove clickno do
		(
			case clickno of
			(
				2:
				(
					if gridPoint.x >= BoundsMinX+GridSpacing*3 then 
					(
						BoundsMaxX = gridPoint.x 
						BoundsMinX = startPoint.x
					)
					else 
					(
						BoundsMinX = gridPoint.x 
						BoundsMaxX = startPoint.x
					)
					if gridPoint.y >= BoundsMinY+GridSpacing*3 then 
					(
						BoundsMaxY = gridPoint.Y 
						BoundsMinY = startPoint.y
					)
					else 
					(
						BoundsMinY = gridPoint.y
						BoundsMaxY = startPoint.y
					)
					BoundsMaxZ = gridPoint.z+GridSpacing*2
					local theBox = [boundsMinX,boundsMinY,boundsMinZ] + (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
					nodeTM.translation = [theBox.x,theBox.y, boundsMinZ]
					local theBBoxSize = [BoundsmaxX,BoundsmaxY,BoundsmaxZ] - [BoundsminX,BoundsminY, BoundsminZ]
					GridSpacing = (length theBBoxSize)/50.0							
				)
				3:
				(
					BoundsMaxZ = amax #(0.5*length (endpoint-gridPoint), gridPoint.z+GridSpacing*2)
					local theBBoxSize = [BoundsmaxX,BoundsmaxY,BoundsmaxZ] - [BoundsminX,BoundsminY, BoundsminZ]
					GridSpacing = (length theBBoxSize)/50.0							
				)
			)
		)	
	) 	
	
	
	on create do
	(
		manipulateMode = true
		delegate.ViewportBoundsEnabled = true
		seed (timestamp())
		randomID = (random 1000 1000000) as string + "_" + (random 1000 1000000) as string
		local theDefaultPath = getIniSetting (getDir #plugcfg + "\\StokeFieldSimPreferences.ini") "Output" "Default"
		if theDefaultPath == "" do theDefaultPath = "$default\\" --(dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\Cache\\"
		outputPath = theDefaultPath
		theDirs = getDirectories (getCachePath() + "StokeField_"+randomID+"\\*")
		theDirNames = for d in theDirs collect
		(
			theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		maxNumber = 0
		for d in theDirNames do
		(
			theNumber = ""
			for i = d.count to 1 by -1 do 
				if findstring "0123456789" d[i] != undefined then theNumber = d[i]+theNumber else exit
			theNumber = theNumber as integer		
			if theNumber > maxNumber do maxNumber = theNumber		
		)
		outputVersion = "v" + (getZeros (maxNumber+1)) + (maxNumber+1) as string
		pathToSave = (getCachePath()+"StokeField_"+randomID+"\\"+outputVersion)
		makedir pathToSave all:true
		--this.delegate.InitializeCache (pathToSave+"\\"+outputPrefix+"_####.prt")
		this.delegate.SequencePath = pathToSave+"\\"+outputPrefix+"_####.vdb"
		--this.delegate.SequenceCacheCapacityMB = 4096
		this.holder = SimEmberFactory.CreateSimEmberHolder()
		
		local outID = this.holder.initMagmaHolder.CreateNode "Output"
		this.holder.initMagmaHolder.SetNodeProperty outID "channelName" "Velocity"
		this.holder.initMagmaHolder.SetNodeProperty outID "channelType" "float32[3]"
		
		local outID = this.holder.initMagmaHolder.CreateNode "Output"
		this.holder.initMagmaHolder.SetNodeProperty outID "channelName" "Density"
		this.holder.initMagmaHolder.SetNodeProperty outID "channelType" "float32[1]"
		
		local outID = this.holder.magmaHolder.CreateNode "Output"
		this.holder.magmaHolder.SetNodeProperty outID "channelName" "Velocity"
		this.holder.magmaHolder.SetNodeProperty outID "channelType" "float32[3]"
		
		local outID = this.holder.magmaHolder.CreateNode "Output"
		this.holder.magmaHolder.SetNodeProperty outID "channelName" "Density"
		this.holder.magmaHolder.SetNodeProperty outID "channelType" "float32[1]"
		
		this.delegate.ViewportMaskChannel = ""
		this.delegate.ViewportColorChannel = ""
		this.delegate.ViewportVectorChannel = ""	
		
		this.delegate.Playbacktime.controller = bezier_float()
		
		--animate the force UI updates parameter
		local floatEx = float_expression()
		forceUIUpdates.controller = floatEx
		floatEx.setExpression "F"
		
		local theVal=getIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level"
		if theVal != "" do try(StokeGlobalInterface.LoggingLevel = theVal as name)catch()		
	)
	on load do
	(
		if this.delegate.Playbacktime.controller == undefined do 
			this.delegate.Playbacktime.controller = bezier_float()
		
		--animate the force UI updates parameter
		local floatEx = float_expression()
		forceUIUpdates.controller = floatEx
		floatEx.setExpression "F"	
	)
)




callbacks.removeScripts id:#StokeFieldSimReset
callbacks.addScript #systemPreReset "for o in (GetClassInstances StokeFieldSim) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetCache() )" id:#StokeFieldSimReset
callbacks.addScript #filePreOpen "if callbacks.notificationParam() != 2 do for o in (GetClassInstances StokeFieldSim) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetCache() )" id:#StokeFieldSimReset
callbacks.addScript #systemPreNew "if callbacks.notificationParam() == 1 do (for o in (GetClassInstances StokeFieldSim) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetCache() ))" id:#StokeFieldSimReset
callbacks.addScript #preSystemShutdown "::StokeFieldSimulationShutdownCallback()" id:#StokeFieldSimReset