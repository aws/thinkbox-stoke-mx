-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
global KrakatoaPRTLoader, KrakatoaDeleteModifier--, MagmaModifier
global StokeFieldSIM,Stoke_Field --pre-declaring class names that will be defined later...
global StokeParticleSimulationShutdownCallbacksArray, StokeParticleSimulationShutdownCallback

global StokeWarnAboutOverwriting = execute (getIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Options" "WarnAboutOverwriting")
if StokeWarnAboutOverwriting == OK do StokeWarnAboutOverwriting = true


if ::StokeParticleSimulationShutdownCallbacksArray ==undefined do ::StokeParticleSimulationShutdownCallbacksArray= #()
fn StokeParticleSimulationShutdownCallback =
(
	local theCallbacks = for i in ::StokeParticleSimulationShutdownCallbacksArray where i.stokePlugin.DoGetFlushFlag() collect i
	if theCallbacks.count > 0 do
	(
		local q = querybox ( theCallbacks.count as string + " Stoke Particle Simulation"+ (if theCallbacks.count == 1 then " is" else "s are") +" still caching!\n\nClick [Yes] to DISCARD the unsaved data and close 3ds Max.\nClick [No] to WAIT until all data is cached to disk.") title:"STOKE MX: Asynchronous Cache Still Active!"
		if not q do 
		(
			for i in theCallbacks do
			(
				i.stokePlugin.delegate.FlushParticleCache()
				i.stokePlugin.DoSetFlushFlag false
			)
		)
	)
)
	
(
	-- This file depends on StokeSimulator.ms, so we include it here.
	try(
		local rootFolder = StokeGlobalInterface.HomeDirectory
		
		if rootFolder != undefined do (
			fileIn (rootFolder + "Scripts\\StokeSimulator.ms")
		)
	)catch(
		format "StokeSimulator.ms error: %\n" ( getCurrentException() )
	)
)


-- Callback struct invoked by StokeSimulator at the end of each frame
struct StokeCallback(
	stokePlugin = undefined,
	stokeSim = undefined,
	
	particleSources = undefined,
	velocityFields = undefined,
	
	storageCounter = 0,
	updateCounter = 0,
	
	asyncCacheStart = 0,
	
	fn onFrameUpdate = 
	(
		if stokePlugin.stopSimDueToMagmaError then
		(
			stokePlugin.stopSimDueToMagmaError = false
			false
		)
		else
		(
			local theTime = stokeSim.GetCurrentTime()
			storageCounter += 1
			if storageCounter == stokePlugin.memoryCacheStep do
			(
				storageCounter = 0				
				if not stokePlugin.useCacheStartTime or theTime >= stokePlugin.CacheStartTime do
					stokePlugin.delegate.AddParticleSet ( stokeSim.GetCurrentParticles() ) theTime --this caches to memory and allows PRT saving 
			)
			
			if stokePlugin.updateViews and stokePlugin.updateViewsEvery == updateCounter do
			(
				sliderTime = theTime -- -1 --update to previous frame to avoid PRT access error
				updateCounter = 0
			)
			updateCounter += 1
			if ( stokePlugin.onFrameUpdateUI theTime ) then 
			(
				-- Add code for updating animated parameters here.
				at time theTime 
				( 
					stokePlugin.updateParticleSourceRates particleSources
					stokePlugin.updateVelocitySourceScales velocityFields
				)
				
				true
			) else false
		)
	),
	
	fn FlushReadyCallback =
	(
		if StokeGlobalInterface.LoggingLevel != #none do
			format "--ASYNC DISK CACHE FINISHED SAVING IN % SECONDS.\n\n" ((timestamp()-asyncCacheStart)/1000.0)
		stokePlugin.delegate.SetSerializerCallback undefined
		stokePlugin.DoSetFlushFlag false
		stokePlugin.DoUpdateMemoryCount()
		local theIndex =findItem ::StokeParticleSimulationShutdownCallbacksArray this
		if theIndex > 0 do deleteItem ::StokeParticleSimulationShutdownCallbacksArray theIndex 
		
		-- If there isn't any node pointing at the Stoke object we try to reduce the memory usage by resetting the cache. This typically occurs when
		-- the scene is reset while an asynchronous flush is pending.
		if (refs.dependentNodes stokePlugin baseObjectOnly:true).count == 0 do (
			stokePlugin.delegate.ResetParticleCache()
		)
	),
	
	fn FlushFrameReadyCallback theFile =
	(
		if findItem #(#progress, #stats, #debug) StokeGlobalInterface.LoggingLevel > 0 do
			format "  --ASYNC DISK CACHE Saved File [%]\n" theFile
		stokePlugin.DoUpdateMemoryCount()
	)	
	
)

plugin geometry Stoke
name:"Stoke"
category:"Stoke"
classid:#(0x2cf34591, 0x38e6602)
extends:StokeBase
replaceui:true
(
	local paramsRollout, paramsDisplayRollout, paramsDistributionRollout, paramsVelocityFieldRollout, cacheParamsRollout, paramsLifespanRollout, paramsTimingRollout, helpRollout, presetsRollout
	local availableChannels = #()
	local availableChannelNames = #()
	local lockUpdates = false
	local RCMenuSourceObject
	local Stoke_Progress_Cancel = false
	local isSimulating = false
	--local theMaxVersion = 0
	local theSim = undefined
	local lastSimHash=0
	local isAsyncFlushActive = false
	local isAsyncFlushCancelled = false

	--For Magma
	local LastErrorID = -100
	local LastErrorMsg = undefined

	local LastBirthErrorID = -100
	local LastBirthErrorMsg = undefined

	local stopSimDueToMagmaError = false
	
	fn DoUpdateMemoryCount = (
		this.cacheParamsRollout.updateMemoryCount()
	)	
	
	fn DoSetFlushFlag arg = (
		this.isAsyncFlushActive = arg
	)
	
	fn DoGetFlushFlag = (
		this.isAsyncFlushActive 
	)
	
	on detachedFromNode theNode do (
		-- If we don't have an asynchronous flush pending and this is the last node pointing at the Stoke object we try to reduce the memory usage
		-- by resetting the cache.
		if not this.isAsyncFlushActive and (refs.dependentNodes this baseObjectOnly:true) == 1 do (
			this.delegate.ResetParticleCache()
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
			local theRollouts = #(paramsRollout, paramsDisplayRollout, paramsDistributionRollout, paramsVelocityFieldRollout, cacheParamsRollout, paramsLifespanRollout, paramsTimingRollout, helpRollout, presetsRollout)
			for aRollout in theRollouts do --loop through all rollouts, and set their autoLayoutOnResize property to the value in the global variable determined in the Stoke startup script:
				setProperty aRollout #autoLayoutOnResize ::Stoke_autoLayoutOnResizeOn
		)
	)		
	
	fn defineRCMenu =
	(
		rcmenu Stoke_Object_RCMenu
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
		Stoke_Object_RCMenu			
	)
	
	fn defineRCAgeMenu =
	(
		rcmenu Stoke_Age_RCMenu
		(
			menuItem mnu_deleteByAge "Delete Using Age And LifeSpan Channels"
			menuItem mnu_deleteByNormAge "Delete Using NormalizedAge Channel"
			on mnu_deleteByAge picked do
			(
				paramsLifespanRollout.addDeleteByAgeModifiers mode:#Age
			)
			on mnu_deleteByNormAge picked do
			(
				paramsLifespanRollout.addDeleteByAgeModifiers mode:#normAge
			)
		)	
		Stoke_Age_RCMenu				
	)
	
	fn defineChannelTypeRCMenu =
	(
		rcmenu Stoke_ChannelType_RCMenu
		(
			menuItem mnu_type16 "Set Selected Float* Channels To 16 bit"
			menuItem mnu_type32 "Set Selected Float* Channels To 32 bit"
			separator sep_10
			menuItem mnu_selectFloat16 "Select Float16 Channels"
			menuItem mnu_selectFloat32 "Select Float32 Channels"
			separator sep_20
			menuItem mnu_selectVector16 "Select Float16[3] Channels"
			menuItem mnu_selectVector32 "Select Float32[3] Channels"
			separator sep_30
			menuItem mnu_selectInvert "INVERT Channel Selection"
			menuItem mnu_selectAll "Select ALL Channels"
			
			on mnu_selectFloat32 picked do
			(
				paramsDistributionRollout.selectChannelsByType type:32 vector:false
			)
			on mnu_selectFloat16 picked do
			(
				paramsDistributionRollout.selectChannelsByType type:16 vector:false
			)

			on mnu_selectVector16 picked do
			(
				paramsDistributionRollout.selectChannelsByType type:16 vector:true
			)
			on mnu_selectVector32 picked do
			(
				paramsDistributionRollout.selectChannelsByType type:32 vector:true
			)			
			
			on mnu_type16 picked do
			(
				paramsDistributionRollout.switchChannelTypeTo type:16
			)
			on mnu_type32 picked do
			(
				paramsDistributionRollout.switchChannelTypeTo type:32
			)
			on mnu_selectAll picked do
			(
				paramsDistributionRollout.selectAllChannels()
			)
			on mnu_selectInvert picked do
			(
				paramsDistributionRollout.InvertSelectedlChannels()
			)
		)	
		Stoke_ChannelType_RCMenu			
	)
	
	fn defineMagmaRCMenu =
	(
		rcmenu Stoke_Per_Step_Magma_RCMenu
		(
			menuItem mnu_enableMagmaChannels "Enable Channels Used By Magma"
			on mnu_enableMagmaChannels picked do
			(
				paramsRollout.enableMagmaChannels()
			)
		)
		Stoke_Per_Step_Magma_RCMenu
	)
	
	
	parameters cacheParams rollout:cacheParamsRollout
	(
		randomID type:#string default:"12345_67890"
		outputPath type:#string default:"" ui:edt_outputPath
		outputVersion type:#string default:"v001" ui:edt_outputVersion
		outputPrefix type:#string default:"Stoke" ui:edt_outputPrefix
		
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
		
		useEmitEndTime type:#boolean default:false animatable:false ui:chk_useEmitEndTime
		emitEndTime type:#integer default:100 animatable:false ui:spn_emitEndTime
		
		rateMode type:#integer default:1 ui:ddl_rateMode
		rate type:#integer default:100 ui:spn_rate
		
		updateViews type:#boolean default:false animatable:false ui:chk_updateViews
		updateViewsEvery type:#integer default:5 ui:spn_updateViewsEvery
		
		on rate set val do paramsRollout.updateUI()
		on startTime set val do if startTime > endTime do endTime = startTime
		on endTime set val do if startTime > endTime do startTime = endTime
	)
	
	parameters paramsLifespan rollout:paramsLifespanRollout
	(
		lifeSpan type:#integer default:25.0 ui:spn_lifeSpan
		lifeSpanVar type:#integer default:5.0 ui:spn_lifeSpanVar
		useLifeSpan type:#boolean default:false animatable:false ui:chk_useLifeSpan
	)
	
	parameters paramsDistribution rollout:paramsDistributionRollout
	(
		useViewportParticles type:#boolean default:false ui:chk_useViewportParticles
		jitterRadius type:#float default:1.0 
		distMode type:#integer default:1 
		distSources type:#nodeTab tabSizeVariable:true
		distSourcesOnOff type:#boolTab tabSizeVariable:true
		
		distRates type:#intTab tabSizeVariable:true
		distSourcesSelectionType type:#stringTab tabSizeVariable:true
		distJitterRadius type:#floatTab tabSizeVariable:true
		distSeedAsRate type:#boolTab tabSizeVariable:true
		distUseNewParticlesOnly type:#boolTab tabSizeVariable:true
		distVolumeSpacing type:#floatTab tabSizeVariable:true animatable:false
		perObjectRate type:#integer default:100 ui:spn_perObjectRate animatable:false
		perObjectJitter type:#float default:1.0 ui:spn_perObjectJitter animatable:false
		perObjectVolumeSpacing type:#float default:1.0 ui:spn_perObjectVolumeSpacing animatable:false
		
		randomSeed type:#integer default:12345 ui:spn_randomSeed
		incrementRandomSeed type:#boolean default:true ui:chk_incrementRandomSeed
		channelsToSave type:#stringTab tabSizeVariable:true
		geometryMode type:#stringTab tabSizeVariable:true
		
		allowMultiPickDistribution type:#boolean default:false ui:chk_allowMultiPickDistribution
		channelsToSaveFormat type:#stringTab tabSizeVariable:true
		
		on perObjectRate set val do
		(
			if not lockUpdates do 
				paramsDistributionRollout.updatePerObjectRate()
		)			
	)
	
	parameters paramsVelocityField rollout:paramsVelocityFieldRollout
	(
		VelocitySources type:#nodeTab tabSizeVariable:true
		VelocitySourcesOnOff type:#boolTab tabSizeVariable:true
		velocityScales type:#floatTab tabSizeVariable:true 
		
		useGlobalGrid type:#boolean default:true ui:chk_useGlobalGrid animatable:false
		gridSizes type:#worldunitsTab tabSizeVariable:true animatable:false
		gridPaddings type:#intTab tabSizeVariable:true animatable:false
		fluidMotions type:#boolTab tabSizeVariable:true  animatable:false
		
		gridSize type:#worldunits default:10.0 animatable:false
		gridPadding type:#integer default:5 animatable:false
		fluidMotion type:#boolean default:false animatable:false
		--useViewportParticlesField type:#boolean default:false animatable:false ui:chk_useViewportParticlesField
		VelocityScale type:#float default:1.0 ui:spn_VelocityScale animatable:false
		on VelocityScale set val do
		(
				paramsVelocityFieldRollout.updatePerObjectScale()
		)			
	)
	
	parameters paramsDisplay rollout:paramsDisplayRollout
	(
		viewPercentage type:#float default:100 ui:spn_viewPercentage
		viewLimitEnabled type:#boolean default:false ui:chk_viewLimitEnabled
		viewLimit type:#integer default:1000 animatable:false ui:spn_viewLimit
		
		ViewVectorNormalize type:#boolean default:false ui:chk_ViewVectorNormalize
		viewVectorScale type:#float default:1.0 ui:spn_viewVectorScale 
		iconSize type:#float default:30.0 ui:spn_iconSize
		
		forceUIUpdates type:#float default:0 ui:spn_forceUIUpdates	--this parameter triggers UI updates on time slider changes in place of a time change callback to work around issues in 3ds Max 2019.3 and higher
		
		on viewVectorScale set val do this.delegate.viewportVectorScale = val
		on ViewVectorNormalize set val do this.delegate.viewportVectorNormalize = val
		
		on iconSize set val do 
		(
			this.delegate.iconSize = val
		)
		on viewPercentage set val do 
		(
			this.delegate.ViewportPercentage = val
		)
		on viewLimit set val do
		(
			this.delegate.ViewportLimit = val
		)
		on viewLimitEnabled set val do
		(
			this.delegate.UseViewportLimit = val
		)
		on forceUIUpdates get val do --this event handlers is called on time slider changes and refreshes any UI controls that do not support auto-updating
		(
			cacheParamsRollout.updateMemoryCount()
			paramsDistributionRollout.updateObjectsListAnimation()
			paramsVelocityFieldRollout.updateObjectsListAnimation()
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
		local theDefaultPath = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\Stoke\\Cache\\"
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
		makedir (theCachePath+"Stoke_"+randomID+"\\"+theVersion) all:true		
		outputVersion = theVersion
		cacheParamsRollout.setCachePath()
	)
	
	fn getNewVersionFolder =
	(
		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + "Stoke_"+randomID+"\\*")
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
	
	fn createVersionRCMenu =
	(
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"

		local nextVersion = getNewVersionFolder()
			
		txt += "menuItem mnu_NextVersion \"New Version ["+nextVersion+"]\" \n"
		txt += "on mnu_NextVersion picked do (selection[1].createNewVersion \""+ nextVersion +"\" )\n"
		txt += "separator sep_10\n"

		local theCachePath = getCachePath()
		local theDirs = getDirectories (theCachePath + "Stoke_"+randomID+"\\*")
		local theDirNames = for d in theDirs collect
		(
			local theFS = (filterString d "\\")
			theFS[theFS.count]
		)
		
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
		if selection[1] == undefined or classof selection[1] != Stoke do return false
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"
			
		txt += "menuItem mnu_defaultName \"Default Name 'Stoke'\" \n"
		txt += "on mnu_defaultName picked do (selection[1].outputPrefix = \"Stoke\")\n"
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
		if selection[1] == undefined or classof selection[1] != Stoke do return false
		local isTime = false
		local isInDelegate = false
		case type of
		(
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
			#emitEndTime : (
						presetName = "emitEndTime"
						theParameter = "emitEndTime"
						isTime = true
					)						
			#rate: (
						presetName = "rate"
						theParameter = "rate"
					)						
			#JitterRadius: (
						presetName = "JitterRadius"
						theParameter = "JitterRadius"
					)	
			#randomSeed: (
						presetName = "randomSeed"
						theParameter = "randomSeed"
					)
			#VelocityScale: (
						presetName = "VelocityScale"
						theParameter = "VelocityScale"
				)
			#gridSize: (
						if useGlobalGrid then
						(
							presetName = "gridSize"
							theParameter = "gridSize"				
						)
						else
						(
							local theSelectionIndex = (getListViewSelection paramsVelocityFieldRollout.dnc_VelocitySources)[1] 
							if theSelectionIndex == undefined do return false
							presetName = "gridSizes["+theSelectionIndex as string+"]"
							theParameter = "gridSizes["+theSelectionIndex as string+"]"
						)
				)
			#gridPadding: (
						presetName = "gridPadding"
						theParameter = "gridPadding"				
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
			#perObjectJitter: (
						presetName = "perObjectJitter"
						theParameter = "perObjectJitter"				
			)
			#perObjectVolumeSpacing: (
						presetName = "perObjectVolumeSpacing"
						theParameter = "perObjectVolumeSpacing"				
			)
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokePreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
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
				#viewVectorScale: #(1.0,2.0,10.0,24.0,25.0,30.0)
				#perObjectRate: #(100,1000,2000,5000,10000,100000,1000000)
				#memoryCacheStep: #(1,2,3,5,10,15,20)
				#memoryLimit: #(1024,2048,3072,4096,5120,6144,7168,8192,12288,16384,24576,32768,65536)
				#threadLimit: #(0,1,2,4,8,12,16,24,32)
				#perObjectJitter: #(1.0,10.0,100.0,10000.0)
				#perObjectVolumeSpacing: #(1.0,2.0,5.0,10.0)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokePreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("selection[1]." + theDelText + theParameter)
		global Stoke_Presets_RCMenu
		local txt = "rcmenu Stoke_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try(selection[1].paramsRollout.updateUI())catch()\n"
		txt+= "try(selection[1].cacheParamsRollout.updateUI())catch()\n"
		txt+= "try(selection[1].paramsVelocityFieldRollout.updateUI())catch()\n"
		
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
			
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokePreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
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
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokePreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
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
		
		--if theMaxVersion > 12000 do 
			windows.processPostedMessages()
		
		--paramsRollout.prg_progress.value = progressValue
		paramsRollout.btn_cancel.text = "STOP | Frame " + (theTime.frame as integer) as string +" ("+ progressValue as string + "%)"
		cacheParamsRollout.updateMemoryCount()
		--if theMaxVersion < 13000 do Stoke_Progress_Cancel = not (progressUpdate progressValue)
		not Stoke_Progress_Cancel
	)
	
	fn getHashSum =
	(
		local thePropString = "" as stringStream
		for p in getPropNames this where findItem #(#memoryCacheStep,#memoryLimit, #threadLimit, #subSteps, #updateViews, #updateViewsEvery,#rate,#endtime,#viewPercentage,#viewLimitEnabled,#viewLimit,#ViewVectorNormalize,#viewVectorScale,#iconSize,#velocityScales,#VelocityScale) p == 0 do
		(
			if findItem #(#distSources, #VelocitySources) p > 0 then
			(
				local theProp = for i in (getProperty this p) collect if isValidNode i then i.inode.handle else -1
				format "%\n" theProp to:thePropString 
			)
			else
				format "%\n" (getProperty this p) to:thePropString 
		)
		(dotNetObject "System.String" thePropString).GetHashCode()
	)
	
	fn collectVelocitySources = 
	(
		local result = #()
		
		for i = 1 to VelocitySources.count where VelocitySourcesOnOff[i] != false and isValidNode VelocitySources[i] do 
		(
			local sourceNode =  VelocitySources[i]
			local theField = undefined
			local theFieldScale = velocityScales[i]
			
			case ( StokeGlobalInterface.GetVelocityType sourceNode ) of ( --new in Stoke 2.0
				#particles:
				(
					local theGridSize = gridSizes[i]
					if theGridSize == undefined or useGlobalGrid do theGridSize = gridSize
					local theGridPadding = gridPaddings[i]
					if theGridPadding == undefined or useGlobalGrid do theGridPadding = gridPadding
					local theFluidMotion = fluidMotions[i]				
					if theFluidMotion == undefined or useGlobalGrid do theFluidMotion = fluidMotion
					
					/*
					format "gridSize=%\n" theGridSize
					format "gridPadding=%\n" theGridPadding					
					format "fluidMotion=%\n" theFluidMotion					
					*/
					theField = try(StokeGlobalInterface.CreateParticleVelocityField sourceNode theGridSize BoundsPadding:theGridPadding RemoveDivergence:theFluidMotion)catch(undefined)
				)
				#field: --handle Stoke fields, return undefined if it fails due to a missing Velocity channel
				(
					theField = try(StokeGlobalInterface.CreateReflowField sourceNode)catch(undefined)
				)
				#invalid: --do nothing if the object is not a valid source
				(
					
				)
				default: --anything else
				(
					theField = try(StokeGlobalInterface.CreateReflowField sourceNode)catch(undefined)
				)
			)
			if theField != undefined do theField.VelocityScale = theFieldScale
			result[i] = theField
		)
		if result.count == 0 do
			result[1] = StokeGlobalInterface.CreateDummyReflowField()
		result
	)
	
	fn collectParticleSources = 
	(
		result = #()

		for i = 1 to DistSources.count where isValidNode DistSources[i] and distSourcesOnOff[i] != false do 
		(
			local sourceNode = DistSources[i]
			local pg = undefined
			
			case ( StokeGlobalInterface.GetSourceType sourceNode ) of 
			(
				#particles: 
				(
					local theJitterRadius = distJitterRadius[i] 
					if classof theJitterRadius != Float do theJitterRadius = 0.0
					
					pg = StokeGlobalInterface.CreateKrakatoaGenerator sourceNode JitterRadius:theJitterRadius IgnoreIDs:(not (distUseNewParticlesOnly[i] != False))
				)
				#geometry:
				(
					local theGeometryMode = geometryMode[i]
					if theGeometryMode == undefined or theGeometryMode == "" do theGeometryMode = "Surface"
						
					local theVSpacing = distVolumeSpacing[i]
					if theVSpacing == undefined do theVSpacing = 1.0
					if theVSpacing < 0.01 do theVSpacing = 0.01
						
					local theSelectionType = DistSourcesSelectionType[i] as string
					
					local theJitterRadius = distJitterRadius[i] 
					if classof theJitterRadius != Float do theJitterRadius = 0.0
						
					pg = StokeGlobalInterface.CreateGeometryGenerator sourceNode mode:theGeometryMode SelectionType:theSelectionType VolumeSpacing:theVSpacing VertexJitterRadius:theJitterRadius
				)
				#fumefx:
				(
					pg = StokeGlobalInterface.CreateFumeFXGenerator sourceNode
				)
			)
			
			if pg != undefined do at time startTime
			(	
				pg.InitialLifespan = [ 1.0*(lifeSpan-lifeSpanVar) / FrameRate, 1.0*(lifeSpan+lifeSpanVar) / FrameRate ]
				pg.RandomSeed = randomSeed + (if incrementRandomSeed then (startTime) as integer else 0) + i
				pg.DiffusionConstant = 0.0
			)
			
			result[i] = pg
		)
		result
	)
	
	fn collectParticleChannels particleSources = 
	(
		local result = #() 
		local alreadyProcessed = #("ID","Velocity","Age","LifeSpan","NormalizedAge","Position")
	
		for pg in particleSources where pg != undefined do 
		(
			local theChannels = try(for i in pg.AvailableChannels collect filterString i " []")catch(#())
			
			for aChannel in theChannels where findItem channelsToSave aChannel[1] > 0 do
			(
				if findItem alreadyProcessed aChannel[1] == 0 do
				(
					local formatType = "16"
					for i in channelsToSaveFormat where matchPattern i pattern:(aChannel[1]+"*") do
					(
						if matchPattern i pattern:"*32*" do formatType = "32"
					)
					local theResult = aChannel[1] + " "
					theResult += case of
					(
						(matchPattern aChannel[2] pattern:"float*"): ("float"+formatType)
						(matchPattern aChannel[2] pattern:"int*"): ("int32")
						default: aChannel[2]
					)
					if aChannel[3] != undefined then
						theResult += "[" +aChannel[3] as string + "]"
					else
						theResult += "[1]"

					append result theResult
					append alreadyProcessed aChannel[1]
				)
			)--end aChannel loop
		)--end pg loop
		
		for theMagma in #(this.delegate.BirthMagma) do
		(
			for i in theMagma.GetNodes() do
			(
				if theMagma.getNodeType i == "Output" do 
				(
					local theChannel = theMagma.getNodeProperty i "channelName"
					local theChannelType = filterString (theMagma.getNodeProperty i "channelType") "[]"
					if theChannel != "" and findItem alreadyProcessed theChannel == 0 and findItem channelsToSave theChannel > 0 do 
					(
						local formatType = "16"
						for i in channelsToSaveFormat where matchPattern i pattern:(theChannel+"*") do
						(
							if matchPattern i pattern:"*32*" do formatType = "32"
						)
						local theResult = theChannel+" "
						theResult += case of
						(
							(matchPattern theChannelType[1] pattern:"float*"): ("float"+formatType)
							(matchPattern theChannelType[1] pattern:"int*"): ("int32")
							default: theChannelType[1]
						)
						if theChannelType[2] != undefined then
							theResult += "[" +theChannelType[2] as string + "]"
						else
							theResult += "[1]"						

						append alreadyProcessed theChannel
						append result theResult
					)
				)
			)
		)			
		result
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
			particleSources[i].InitialLifespan = [ 1.0*(lifeSpan-lifeSpanVar) / FrameRate, 1.0*(lifeSpan+lifeSpanVar) / FrameRate ]
			
			--format "%: %\n" currentTime particleSources[i].GeneratorRate
			
		)
		OK
	)
	
	-- Updates the (potentially animated) scale for velocity sources. Should be called within an `at time #` block.
	fn updateVelocitySourceScales velocitySources = 
	(
		for i = 1 to velocitySources.count where velocitySources[i] != undefined and VelocityScales[i] != undefined do 
			velocitySources[i].VelocityScale = VelocityScales[i]
		OK
	)
	
	rollout helpRollout "Help" rolledup:true
	(
		label lbl_about01 "PARTICLE SIMULATOR"
		label lbl_about10 "STOKE Particle Reflow Tools"
		label lbl_about20 ""
		
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
			shellLaunch "https://docs.thinkboxsoftware.com/products/stoke/2.3/1_Documentation/manual/reflowtools/particle_simulator.html" ""
		)
		
		
		fn updateLayout =
		(
			btn_openOnlineHelp.FlatStyle = btn_openOnlineHelp.FlatStyle.System
			btn_openOnlineHelp.width = helpRollout.width-14
			ddl_loggingLevel.width = helpRollout.width-64
			btn_openOnlineHelp.pos.x = 7
			ddl_loggingLevel.pos.x = 60
		)
		
		on helpRollout open do
		(
			enableAutoLayout()
			updateLayout()
				
			lbl_about20.text = try("Version " + StokeGlobalInterface.Version)catch("")
			
			ddl_loggingLevel.selection = findItem (for i in ddl_loggingLevel.items collect i as name) StokeGlobalInterface.LoggingLevel
		)
	)
	
	
	rollout presetsRollout "Presets" rolledup:true
	(
		dotNetControl btn_loadPreset "Button" text:"LOAD Preset" across:2 offset:[-5,0] height:22
		dotNetControl btn_savePreset "Button" text:"SAVE Preset" offset:[5,0] height:22
		on btn_loadPreset click arg do
		(
			global StokeMX_PresetDialogMode = #load
			global StokeMX_PresetObject = this
			local rootFolder = StokeGlobalInterface.HomeDirectory
			if rootFolder != undefined do 
				fileIn (rootFolder + "Scripts\\StokePresetsManager.ms")
		)
		on btn_savePreset click arg do
		(
			global StokeMX_PresetDialogMode = #save
			global StokeMX_PresetObject = this
			local rootFolder = StokeGlobalInterface.HomeDirectory
			if rootFolder != undefined do 
				fileIn (rootFolder + "Scripts\\StokePresetsManager.ms")
		)		
		fn updateLayout =
		(
			btn_loadPreset.FlatStyle = btn_savePreset.FlatStyle = btn_loadPreset.FlatStyle.System
			btn_loadPreset.width = btn_savePreset.width = (presetsRollout.width-14)/2
			btn_loadPreset.pos.x = 7
			btn_savePreset.pos.x = btn_loadPreset.width+8
		)
		on presetsRollout open do
		(
			enableAutoLayout()
			updateLayout()
		)
	)
	
	rollout cacheParamsRollout "Saving and Caching"
	(
		edittext edt_outputPath offset:[-15,-3] fieldwidth:140 align:#left across:2
		button btn_getOutputPath "..." width:16 height:17 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,11,11,11,11) tooltip:"Click to open a menu with various Base Output Path operations...\n\nEnter a user-defined path in the text field to the left - either using an explicit path or the following symbolic tokens:\n\n$default = C:\\Users\\<UserName>\\AppData\\Local\\Thinkbox\\Stoke\\Cache\\ \n$scene = 3ds Max Scene File Name\n$user = System User Name"
		
		edittext edt_outputVersion offset:[-15,-3] fieldwidth:60 align:#left across:4
		button btn_outputVersion ">"  width:16 height:17 align:#center offset:[7,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Click to create a new Version folder or select an existing Version folder..."
		
		edittext edt_outputPrefix offset:[12,-3] fieldwidth:60 align:#center 
		button btn_outputPrefix ">"  width:16 height:17 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"" enabled:false
		
		edittext edt_driveinfo offset:[-3,-3] fieldwidth:158 align:#center readonly:true
		
		group "Cache Options"		
		(
			progressbar prg_savingStatus across:3 width:11 height:22 align:#left offset:[-6,-4] color:(white*0.5) value:100
			checkbutton chk_saveResultsToDisk ">Use Disk Cache" offset:[-3,-4] align:#center width:119 height:22 across:2 tooltip:"If checked, the particle simulation will be saved to a PRT file sequence asynchronously during the simulation.\n\nThe disk cache will be used to restore the memory cache on scene load.\n\nThe resulting sequence can also be loaded using a Krakatoa PRT Loader - see the [+] button to the right."
			button btn_createPRTLoader "+" width:18 height:22 align:#right offset:[7,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,13,13,21,21) tooltip:"Click to create a new Krakatoa PRT Loader object with the current PRT sequence."
			
			button btn_stopAsyncCache "CANCEL SAVING" offset:[-4,-27] visible:false align:#center width:119 height:22 tooltip:"Click to cancel the asynchronous saving of PRT files by the background thread."
			
			spinner spn_memoryCacheStep "Cache Nth " fieldwidth:40 range:[1,20,0] type:#integer align:#right offset:[56,-3] across:2 tooltip:"Defines the Memory Cache step.\n\nStoke can interpolate the velocities by ID between frames, so storing a sub-set of frames is often enough to get a fair idea of the simulation's look."
			button btn_memoryCacheStep_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Memory Cache step.\n\nStoke can interpolate the velocities by ID between frames, so storing a sub-set of frames is often enough to get a fair idea of the simulation's look."

			spinner spn_threadLimit "Cache Threads " fieldwidth:40 range:[0,sysinfo.cpucount,0] type:#integer align:#right offset:[56,-3] across:2 tooltip:"Defines the Maximum Number of Threads to use for Saving of PRT files in the Disk Cache.\n\nWhen set to 0, the Number of CPUs/Cores will be used."
			button btn_threadLimit_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Maximum Number of Threads to use for Saving of PRT files in the Disk Cache.\n\nWhen set to 0, the Number of CPUs/Cores will be used."
			
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
			local theTimes = for i in this.delegate.GetParticleCacheTimes() collect (i.frame as integer)
			local theMemTimes = for i in this.delegate.GetMemoryCacheTimes() collect (i.frame as integer)
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
			btn_createPRTLoader.enabled = FranticParticles != undefined and saveResultsToDisk
			updateMemoryCount()
			updateAsyncControlsUI()
			getFreeSpaceOnDisk (getCachePath())
		)
		
		fn createWaitDialog =
		(
			rollout Stoke_SavingPleaseWait_Rollout "STOKE Saving, Please Wait..."
			(
				label lbl_01 "Saving Memory Cache to PRT Files, this can take a while..."
			)
			Stoke_SavingPleaseWait_Rollout
		)		
		
		/*
		fn flushCacheToCurrentPath =
		(
			local Stoke_SavingPleaseWait_Rollout = createWaitDialog()
			createDialog  Stoke_SavingPleaseWait_Rollout 400 25
			try(
				if saveResultsToDisk then
					this.delegate.FlushParticleCache()
				else
				(
					local outPath = getCachePath()  + "Stoke_" +randomID+"\\"+ outputVersion + "\\" + outputPrefix+"_####.prt" 
					this.delegate.WriteParticleCache outPath
				)
				destroyDialog Stoke_SavingPleaseWait_Rollout 
			)catch(
				destroyDialog Stoke_SavingPleaseWait_Rollout 
				messageBox ( getCurrentException() ) title:"Stoke Flush Error"
			)
			updateUI()
		)		
		*/
		
		fn setCachePath =
		(
			local outPath = getCachePath() + "Stoke_" +randomID+"\\" + outputVersion + "\\"
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
				this.delegate.InitializeParticleCache ( outPath+outputPrefix+"_####.prt" )
			)
			else
				this.delegate.InitializeParticleCache ""
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
					local q = (querybox  "Are you sure you want to RESET the Cache?\n\nAny unsaved data will be lost!" title:"STOKE MX: RESET Cache?")
				else
					q = true
				if q do
				(
					this.delegate.ResetParticleCache()
					gc light:true
					updateUI()
				)
			)
			else
			(
				if (querybox  "Are you sure you want to RESET the Memory Cache?\n\nDisk Cache Is Disabled!\nAny unsaved data will be lost!" title:"STOKE MX: RESET Memory Cache?") do 
				(
					this.delegate.ResetParticleCache()
					updateUI()
				)
			)
		)
		
		fn saveCacheToPRTSequence fromMemory:false =
		(
			local theNewPath = getSaveFileName caption:"Save the Stoke Memory Cache to a New PRT File Sequence" filename:"Stoke_" types:"Krakatoa PRT Files (*.prt)|*.prt"
			if theNewPath != undefined do
			(
				local Stoke_SavingPleaseWait_Rollout = createWaitDialog()
				createDialog  Stoke_SavingPleaseWait_Rollout 400 25
				this.delegate.WriteParticleCache theNewPath FromMemoryOnly:fromMemory
				destroyDialog Stoke_SavingPleaseWait_Rollout 
				updateUI()
			)
		)
		
		on btn_stopAsyncCache pressed do
		(
			isAsyncFlushCancelled = true
			if StokeGlobalInterface.LoggingLevel != #none do			
				format "--ASYNC DISK CACHE SAVING CANCELLED BY USER!\n"
			local st = timestamp()
			--this.delegate.ResetParticleCache()
			this.delegate.CancelFlushParticleCache()
			if StokeGlobalInterface.LoggingLevel == #debug do			
				format "--Resetting Cache Took % Seconds.\n" ((timestamp()-st)/1000.0)
			/*local st = timestamp()	
			local outPath = getCachePath() + "Stoke_" +randomID+"\\" + outputVersion + "\\"
			this.delegate.InitializeParticleCache ( outPath+outputPrefix+"_####.prt" )
			if StokeGlobalInterface.LoggingLevel == #debug do			
				format "--Cache Path Set To [%] in % seconds.\n" ( outPath+outputPrefix+"_####.prt" ) ((timestamp()-st)/1000.0)*/
			updateUI()
		)
		

		
		fn createClearMemoryMenu =
		(
			rcmenu Stoke_ClearMemory_Menu 
			(
				menuItem mnu_resetCache "RESET the Memory Cache..."
				separator sep_10
				--menuItem mnu_flushCacheToCurrentPath "FLUSH the Memory Cache - Save to current Cache Path"
				--separator set_20
				menuItem mnu_saveCacheToPRTSequence "SAVE the Cache as a NEW PRT Sequence..."
				
				on mnu_resetCache picked do resetCache()
				--on mnu_flushCacheToCurrentPath picked do flushCacheToCurrentPath()
				on mnu_saveCacheToPRTSequence picked do saveCacheToPRTSequence fromMemory:false
			)
			Stoke_ClearMemory_Menu
		)		
		
		on btn_clearParticleCache pressed do
		(
			popupMenu (createClearMemoryMenu()) pos:mouse.screenpos
		)
		on btn_clearParticleCache rightclick do
		(
			popupMenu (createClearMemoryMenu()) pos:mouse.screenpos
		)		
		
		fn createPRTLoader =
		(
			local outPath = getCachePath() + "Stoke_" +randomID+"\\"+ outputVersion + "\\" 
			local loaderObject = KrakatoaPRTLoader name:(uniquename ("PRTL_"+selection[1].name + "_"))
			loaderObject.fileList = #(( outPath+outputPrefix+"_0000.prt" ))
			loaderObject.fileListFlags = #(3)
			--loaderObject.viewLoadMode = viewMode+1
			--loaderObject.enabledInView = viewEnabled
			loaderObject.percentViewport = viewPercentage
			loaderObject.useViewportLimit = true
			loaderObject.viewportLimit = viewLimit
			loaderObject.viewportParticleDisplayMode = 3
			loaderObject.wirecolor = color 255 200 100		
			
			local theStokeSource = $
			if theStokeSource.modifiers.count > 0 do
			(
				local q = yesNoCancelBox "Click [Yes] to INSTANCE all modifiers.\nClick [No] to COPY all modifiers.\nClick [Cancel] to ignore the modifiers." title:"Clone Stoke Object's Modifiers To PRT Loader?"
				if q != #cancel do
				(
					for m = theStokeSource.modifiers.count to 1 by -1 do
					(
						if q == #yes then
							addModifier loaderObject theStokeSource.modifiers[m]
						else
							addModifier loaderObject (copy theStokeSource.modifiers[m])
					)
				)
			)
			select loaderObject					
		)
		
		on btn_createPRTLoader pressed do
		(
			createPRTLoader()
		)		
		on btn_createPRTLoader rightclick do
		(
			createPRTLoader()
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
			local outPath = theCachePath + "Stoke_" +randomID+"\\"+ outputVersion
			if doesFileExist outPath then
				shellLaunch "explorer.exe" outPath
			else
			(
				outPath =  theCachePath + "Stoke_" +randomID+"\\"
				if doesFileExist outPath then 
					shellLaunch "explorer.exe" outPath
				else
					shellLaunch "explorer.exe" theCachePath
			)
		)
		
		fn createOutputPathMenu =
		(
			rcmenu Stoke_OutputPath_Menu 
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
				menuItem mnu_warnAboutOverwriting "Warn About Overwriting Disk Cache Files" checked:(::StokeWarnAboutOverwriting)
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
					setIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Output" "Default" outputPath
				)
				on mnu_getUserDefaultPath picked do 
				(
					local theVal = getIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Output" "Default"
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
			Stoke_OutputPath_Menu
		)
		
		on btn_getOutputPath rightClick do
		(
			local Stoke_OutputPath_Menu = createOutputPathMenu()
			popupMenu Stoke_OutputPath_Menu pos:mouse.screenpos
		)	
		
		on btn_getOutputPath pressed do
		(
			local Stoke_OutputPath_Menu = createOutputPathMenu()
			popupMenu Stoke_OutputPath_Menu pos:mouse.screenpos			
		)		
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)		
		
		on btn_outputVersion pressed do 
		(
			createVersionRCMenu()
			popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)
		
		on btn_outputPrefix pressed do
		(
			if createPrefixRCMenu() do 
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
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
	
	rollout paramsRollout "Simulation"
	(
		label lbl_simoptions "" offset:[0,-24]
		group "Simulation Options"
		(
			dotNetControl btn_simulate "Button" text:"SIMULATE" width:84 height:30 enabled:true align:#left offset:[-6,-4] across:2 tooltip:"Starts a new simulation from the 'Simulate From' frame.\n\nAny previous simulation data in memory or on disk will be replaced."
			dotNetControl btn_resume "Button" text:"RESUME" width:64 height:30 enabled:true align:#right offset:[6,-4] tooltip:"Resumes a stopped simulation from the last processed frame.\n\nExisting simulation data in memory and on disk will be preserved."
			
			--progressbar prg_progress visible:false height:8 offset:[0,-36] color:green width:148 align:#center
			dotNetControl btn_cancel "Button" text:"STOP" width:148 height:30 visible:true align:#center offset:[-10000,-35] tooltip:"Press to stop the simulation.\n\nYou can resume it later using the RESUME button."
			
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
		
		label lbl_birthoptions "" offset:[0,-22]
		group "Particle Birth:"
		(
			checkbox chk_useEmitEndTime "Emit Until" offset:[-5,-4] across:3 align:#left tooltip:"When checked, the simulation results will be cached only after the specified frame."
			spinner spn_emitEndTime "" range:[-100000,1000000,0]  type:#integer fieldwidth:38 offset:[34,-4] align:#right tooltip:"Defines the last frame of particle emission.\n\nUse this to define an Emission Range as a sub-set of the Simulation Range.\n\nNote that you can alternatively keyframe the Rate value to gradually start and stop emission as often as you want within the Emission Range."
			button btn_emitEndTime_Preset ">" width:18 height:16 align:#right offset:[7,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the last frame of particle emission.\n\nUse this to define an Emission Range as a sub-set of the Simulation Range.\n\nNote that you can alternatively keyframe the Rate value to gradually start and stop emission as often as you want within the Emission Range."
			
			dropdownlist ddl_rateMode items:#("Total Rate, Relative Split","Total Rate, Equally Split","Total Rate, Every Source","Absolute Per-Source Rate") width:148 align:#center offset:[1,-3]
			
			spinner spn_rate "Rate/Frame  " range:[0,10E8,1000]  type:#integer fieldwidth:50 offset:[56,-3] across:2 align:#right tooltip:"Defines the number of particles to emit per Emission Range Frame.\n\nNote that you can keyframe this value to gradually start and stop emission as often as you want within the Emission Range."
			button btn_Rate_Preset ">" width:18 height:16 align:#right offset:[6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the number of particles to emit per Emission Range Frame.\n\nNote that you can keyframe this value to gradually start and stop emission as often as you want within the Emission Range."
			
			edittext edt_totalCount "Total Count  " text:"100000" readOnly:true fieldwidth:77 align:#right offset:[6,-3]
		)
		label lbl_magmaoptions "" offset:[0,-22]
		group "Magma Channels"
		(
			progressbar prg_BirthMagmaError value:100 color:green width:8 height:30 offset:[-6,-4] align:#left across:3
			dotNetControl btn_openBirthFlow "Button" text:"Edit BIRTH Magma..." width:124 height:30 enabled:true align:#center offset:[-2,-4] tooltip:"Edit the Magma flow which is evaluated on newborn particles." 
			button btn_BirthMagmaOptions ">" width:15 height:30 align:#right offset:[7,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2)
			
			progressbar prg_magmaError value:100 color:green width:8 height:30 offset:[-6,-4] align:#left across:3
			dotNetControl btn_openSimulationFlow "Button" text:"Edit PER-STEP Magma..." width:124 height:30 enabled:true align:#center offset:[-2,-4] tooltip:"Edit the Magma flow which is evaluated every simulation step." 
			button btn_MagmaOptions ">" width:15 height:30 align:#right offset:[7,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2)

			checkbox chk_usePerStepMagma "Use Per-Step Magma" offset:[-6,-4] align:#left checked:this.delegate.UseMagma tooltip:"When checked, the simulation won't use the per-step MAGMA flow, which may increase performance."
		)
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
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
			
		on btn_MagmaOptions pressed do 
		(
			local Stoke_Per_Step_Magma_RCMenu = defineMagmaRCMenu()
			popupMenu Stoke_Per_Step_Magma_RCMenu position:mouse.screenPos	
		)

		on btn_BirthMagmaOptions pressed do 
		(
			local Stoke_Per_Step_Magma_RCMenu = defineMagmaRCMenu()
			popupMenu Stoke_Per_Step_Magma_RCMenu position:mouse.screenPos	
		)
		
		fn enableMagmaChannels =
		(
			local currentChannels = this.channelsToSave 
			for theMagma in #(this.delegate.SimulationMagma, this.delegate.BirthMagma) do
			(
				for i in theMagma.GetNodes() do
				(
					if theMagma.getNodeType i == "Output" do 
					(
						theChannel = theMagma.getNodeProperty i "channelName"
						if theChannel != "" do appendIfUnique currentChannels theChannel
					)
				)
			)
			this.channelsToSave = currentChannels
			paramsDistributionRollout.updateUI()
			paramsDistributionRollout.updateChannelsList()
		)
			
		fn updateRateTooltip =
		(
			case ddl_rateMode.selection of
			(
				default: (
					ddl_rateMode.tooltip = "Emit the total number of particles per frame specified by the global 'Rate/Frame' value.\n\nSplit that amount between all selected Distribution Sources to produce the proportional rates based on the per-source absolute Rates."
					btn_rate_preset.enabled = spn_rate.enabled = true
				)
				2: (
					ddl_rateMode.tooltip = "For EACH Distribution Source, emit the SAME number of particles per frame by splitting the global 'Rate/Frame' value equally between the active sources.\n\nFor example, if Rate/Frame is 1000 and 5 Sources are selected, emit 200 particles from each object.\n\nThis will ignore the per-source Rates."
					btn_rate_preset.enabled = spn_rate.enabled = true
				)
				3: (
					ddl_rateMode.tooltip = "For EACH Distribution Source, emit the SAME number of particles per frame specified by the global 'Rate/Frame' value.\n\nFor example, if Rate/Frame is 100 and 3 Sources are selected, emit 300 particles, 100 from each.\n\nThis will ignore the per-source Rates."
					btn_rate_preset.enabled = spn_rate.enabled = true
				)
				4: (
					ddl_rateMode.tooltip = "Emit the number of particles specified by each Distribution Source's Rate.\n\nIgnore the global 'Rate/Frame' value."
					btn_rate_preset.enabled = spn_rate.enabled = false
				)
			)
		)
		
		fn updateSimAndResumeState =
		(
			btn_simulate.enabled = (for i = 1 to DistSources.count where isValidNode DistSources[i] and DistSourcesOnOff[i] != False collect i).count > 0--and  (for i = 1 to VelocitySources.count where isValidNode VelocitySources[i] and VelocitySourcesOnOff[i] != false and (WSMSupportsForce VelocitySources[i] or StokeGlobalInterface.GetVelocityType VelocitySources[i] != #invalid ) collect i).count > 0
			btn_resume.enabled = theSim != undefined and theSim.GetCurrentTime() != undefined and theSim.GetCurrentTime() < endTime and getHashSum() == lastSimHash
		)
		
		fn updateUI =
		(
			updateSimAndResumeState()
			seed 12345
			local activeEmittersCount = 0
			for o = 1 to DistSources.count where isValidNode DistSources[o] and distSourcesOnOff[o] != false do activeEmittersCount += 1
			local totalValue = 0
			local theEndTime = endTime
			if useEmitEndTime do theEndTime = emitEndTime 
			for t =startTime to endTime-1 where t <= theEndTime do 
			(	
				local totalRates = 0.0
				at time t 				
				(
					for o = 1 to DistSources.count where isValidNode DistSources[o] and DistSourcesOnOff[o] != False do totalRates += distRates[o]
					for o = 1 to DistSources.count where isValidNode DistSources[o] and DistSourcesOnOff[o] != False do
					(
						totalValue+= case rateMode of
						(
							1: (floor (((((distRates[o] as float)/totalRates)*rate))+0.5) ) as integer
							2: (floor (((1.0*rate)/activeEmittersCount)+0.5) ) as integer
							3: rate
							4: distRates[o]
							default: 0
						)			
					)
				)
				
				local randomLifeSpan = lifeSpan --+ (random -lifeSpanVar lifeSpanVar)
				if useLifeSpan and t-startTime >= randomLifeSpan do
				(
					at time (t-randomLifeSpan) 
					(
						for o = 1 to DistSources.count where isValidNode DistSources[o] and DistSourcesOnOff[o] != False do
						(
							totalValue-= case rateMode of
							(
								1: (floor (((((distRates[o] as float)/totalRates)*rate))+0.5) ) as integer
								2: (floor (((1.0*rate)/activeEmittersCount)+0.5) ) as integer
								3: rate
								4: distRates[o]
								default: 0
							)				
						)
					)
				)
			)--end t loop
			edt_totalCount.caption = if useLifeSpan then  "Last Frame" else "Total Count"
			edt_totalCount.text = (if useLifeSpan then "~" else "") + addCommas (totalValue as string)
			cacheParamsRollout.updateMemoryCacheGraph()
			
			spn_cacheStartTime.enabled = btn_cacheStartTime_Preset.enabled = useCacheStartTime
			spn_updateViewsEvery.enabled = btn_updateViewsEvery_Preset.enabled = updateViews
			spn_emitEndTime.enabled = btn_emitEndTime_Preset.enabled = useEmitEndTime

			paramsRollout.prg_MagmaError.color = if LastErrorID >= 0 then red else green
			paramsRollout.prg_BirthMagmaError.color = if LastBirthErrorID >= 0 then red else green
		)	
		
		on chk_useCacheStartTime changed state do updateUI()
		on chk_updateViews changed state do updateUI()
		on chk_useEmitEndTime changed state do updateUI()
		on spn_subSteps changed val do updateUI()
		
		on ddl_rateMode selected itm do 
		(
			updateRateTooltip()
			updateUI()
			paramsDistributionRollout.refreshRateValues()
		)

		on spn_StartTime changed val do updateUI()
		on spn_EndTime changed val do updateUI()
		
		on spn_emitEndTime changed val do updateUI()
		
		on spn_rate changed val do 
		(
			updateUI()
			paramsDistributionRollout.refreshRateValues()
		)

		on chk_usePerStepMagma changed state do (
			this.delegate.UseMagma = chk_usePerStepMagma.checked
		)

		on btn_openSimulationFlow click arg do (
			local theRollout = ::StokeParticleSimSim_ImplementationObject.openEditor this.delegate.SimulationMagma this 
			theRollout.updateErrorLog NodeID:LastSimErrorID ErrorMessage:LastSimErrorMsg
			--this.delegate.UseMagma = chk_usePerStepMagma.checked = true
		)

		on btn_openBirthFlow click arg do (
			local theRollout = ::StokeParticleSimBirth_ImplementationObject.openEditor this.delegate.BirthMagma this 
			theRollout.updateErrorLog NodeID:LastSimErrorID ErrorMessage:LastSimErrorMsg
		)

		local theParticleSources, theVelocityFields, theExtraChannels
		
		fn simulateFunction mode:#sim =
		(
			--theMaxVersion = (maxVersion())[1] 
			local saveThreads = sysinfo.cpucount/2
			if saveResultsToDisk do
			(
				if saveThreads > threadLimit and threadLimit != 0 do saveThreads = threadLimit 
				try(this.delegate.SetNumSerializerThreads saveThreads)catch()
				if StokeGlobalInterface.LoggingLevel != #none do format "--Asynchronous Saving During Simulation Using % Threads On % Cores.\n" saveThreads sysinfo.cpucount
			)
			
			theParticleSources = collectParticleSources()
			theVelocityFields = collectVelocitySources()
			if (for i in theVelocityFields where i != undefined collect i).count == 0 do
			(
				messagebox "STOKE MX requires at least one valid and active Velocity source to perform a particle simulation.\n\nIt appears that a Field source might have been selected that has no valid Velocity channel.\n\nPlease fix your Field sources and try again!"  title:"STOKE MX: No Valid Velocity Sources Found!"
				return false
			)
			--collect the channels to acquire / simulate with
			theExtraChannels = collectParticleChannels  theParticleSources
			
			--collect the channels to save:
			theChannelsToSaveMap = join #("Position float32[3]") (deepCopy theExtraChannels)
			
			local internalChannels = #(#("Velocity","float16[3]"),#("ID","int64[1]"), #("Age","float16[1]"),#("LifeSpan","float16[1]"),#("NormalizedAge","float16[1]"))
			for i in internalChannels where findItem channelsToSave i[1] > 0 do
			(		
				local theChannel = i[1]
				local theFormat = i[2]
				for j in channelsToSaveFormat where matchPattern j pattern:(theChannel+"*") do 
				(
					if matchPattern j pattern:"*32*" do theFormat = substituteString theFormat "16" "32"
				)
				appendIfUnique theChannelsToSaveMap (theChannel+" "+theFormat)
			)			
			
			--print theChannelsToSaveMap
			
			if mode == #sim then
			(
				lastSimHash = getHashSum()
				
				theSim = StokeSimulator()
				theSim.SetParticleChannels theExtraChannels
				theSim.SetSimulationRange startTime endTime numSubSteps:subSteps
				theSim.EnableParticleDeath useLifeSpan
				theSim.SetBirthMagmaHolder this.delegate.BirthMagma
				theSim.SetSimMagmaHolder this.delegate.SimulationMagma
				
				delegate.SetChannelsToSave theChannelsToSaveMap --set the channels to be saved
				format "--Save Channel Map:\n" 
				for i in theChannelsToSaveMap do
					format "\t--%\n" i
				
				for pg in theParticleSources where pg != undefined do 
					theSim.AddParticleSource pg

				local validFields = 
					for vf in theVelocityFields where vf != undefined collect vf
				
				local theVelocityField = if validFields.count > 1 then 
					( StokeGlobalInterface.CreateAdditiveVelocityField validFields ) 
				else
					( validFields[1] )
					
				theSim.SetVelocitySource theVelocityField
				
				this.delegate.ResetParticleCache()
				if saveResultsToDisk then
				(
					local outPath = getCachePath() + "Stoke_" +randomID+"\\" + outputVersion + "\\"
						
					-- DARCY: I'd prefer not to delete the files (since its 1. a waste of time, and 2. a leaky abstraction about the storage algorithm of the disk cache)
					-- In the future let's have a callback that trims any files that aren't part of the cache to achieve this same effect.
					-- BOBO: Removed the explicit deleting for now, allowing the user to overwrite and keep what was not overwritten (might be useful sometimes).
					
					local theFilesToDelete = getFiles (outPath+outputPrefix+"_*.prt")
					if theFilesToDelete.count > 0 and ::StokeWarnAboutOverwriting != false do
					(
						local theNewVersion = getNewVersionFolder()
						local q = yesnocancelbox ("Existing SIMULATION DATA FOUND in the Output Folder.\nAre you SURE you want to overwrite existing files?\n\nClick [Yes] to OVERWRITE.\nClick [No] to INCREMENT the Cache Version to "+theNewVersion as string+".\nClick [Cancel] to CANCEL the Simulation.\n\nYou can disable this message in\nthe Output Path Menu (folder icon).") title:"Overwrite Disk Cache?"
						case q of
						(
							#yes: () --do nothing
							#no: (
								createNewVersion theNewVersion
								outPath = getCachePath() + "Stoke_" +randomID+"\\" + outputVersion + "\\"	
							) --increment the cache!
							#cancel: return false
						)
					)
					
					this.delegate.SequencePath = ( outPath+outputPrefix+"_####.prt" )
					this.delegate.UseDiskCache = true
					
					/*
					local q = if pathIsNetworkPath outPath and theFilesToDelete.count > 0 then
						querybox ("An existing Disk Cache PRT Sequence with "+theFilesToDelete.count as string+" files exists\nin the output folder, and you are writing to a Network location.\n\nClick [Yes] to DELETE the existing files first.\nDepending on the network speed, this operation could be slow.\n\nIf you answer [No] and later cancel the simulation,\nyou might end up with files from different simulations.\n") title:"STOKE MX: Delete Existing Cache From Network?"
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
							--if theMaxVersion > 12000 do 
							windows.processPostedMessages()
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

			--if theMaxVersion < 13000 do progressStart "Simulating..."
			escapeEnable = false
			isSimulating = true
			Stoke_Progress_Cancel = false
			--btn_resume.visible = btn_simulate.visible = false
			btn_resume.pos.x = -10000
			btn_simulate.pos.x = -10000
			--btn_cancel.visible = true
			btn_cancel.pos.x = 8
			
--			prg_progress.visible = true
			
			-- NB! Must call EndRenderMode with same list as BeginRenderMode
			local activeDistSources = for i = 1 to DistSources.count where isValidNode DistSources[i] and DistSourcesOnOff[i] != False collect DistSources[i]
			if not useViewportParticles do 
				StokeGlobalInterface.BeginRenderMode activeDistSources
			
			local activeVelocitySources = for i = 1 to VelocitySources.count where VelocitySourcesOnOff[i] != false and isValidNode VelocitySources[i] collect VelocitySources[i]
			if not useViewportParticles do
				StokeGlobalInterface.BeginRenderMode activeVelocitySources
			
			local st = timestamp()
			local theCallback = StokeCallback stokePlugin:this stokeSim:theSim particleSources:theParticleSources velocityFields:theVelocityFields
			local result = #success
			local errorMessage = ""

			theSim.SetUseMagma this.delegate.UseMagma
			theSim.SetErrorReporter this.delegate.MagmaErrorReporter
			LastErrorID = -100
			LastBirthErrorID = -100
			theSim.SetStokeObject this
			
			try(
				result = theSim.Simulate frameCallback:(theCallback.onFrameUpdate)
			)catch(
				result = #error
				errorMessage = getCurrentException()
			)
			
			-- NB! Must call EndRenderMode with same list as BeginRenderMode
			if not useViewportParticles do 
				StokeGlobalInterface.EndRenderMode activeDistSources

			if not useViewportParticles do
				StokeGlobalInterface.EndRenderMode activeVelocitySources

			--if theMaxVersion < 13000 do progressEnd()
			
			--btn_resume.visible = btn_simulate.visible = true
			btn_simulate.pos.x = 8
			btn_resume.pos.x = btn_simulate.pos.x + btn_simulate.width + 2
			--btn_cancel.visible = false
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
			local theTimeString = ("Stoke Time: "+(totalTime/1000.0) as string +" sec." )
			pushPrompt theTimeString 
			if StokeGlobalInterface.LoggingLevel != #none do
			(
				format "--Generate Time: % sec.\n" (theSim.totalGenerateTime /1000.0)
				format "--Advect Time: % sec.\n" (theSim.totalAdvectTime /1000.0)
				if this.delegate.useMagma do
				(
					format "  --Magma Step Time: % sec.\n" (theSim.totalMagmaStepTime /1000.0)
					format "  --Advect Time Excluding Per-Step Magma: % sec.\n" ((theSim.totalAdvectTime-theSim.totalMagmaStepTime) /1000.0)
				)
				format "--Update Time: % sec.\n" (theSim.totalUpdateTime /1000.0)
				format "--Delete Time: % sec.\n" (theSim.totalDeleteTime /1000.0)
				format "--Overhead Time: % sec.\n" ((totalTime-theSim.totalGenerateTime-theSim.totalAdvectTime-theSim.totalUpdateTime-theSim.totalDeleteTime) /1000.0)
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
				this.delegate.FlushParticleCacheAsync theCallback.FlushReadyCallback
				this.delegate.SetSerializerCallback theCallback.FlushFrameReadyCallback
				append ::StokeParticleSimulationShutdownCallbacksArray theCallback
			)
			
			paramsDisplayRollout.updateUI()
			
			if result == #error and not IsNetServer() do (
				messageBox errorMessage title:"STOKE MX: Simulation Error"
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
			Stoke_Progress_Cancel = true
		)
		
		fn updateLayout =
		(
			btn_openBirthFlow.FlatStyle = btn_openSimulationFlow.FlatStyle = btn_simulate.FlatStyle = btn_resume.FlatStyle = btn_cancel.FlatStyle = btn_simulate.FlatStyle.System
			btn_resume.width = btn_simulate.width = (paramsRollout.width-14)/2
			btn_simulate.pos.x = if isSimulating then -10000 else 8
			btn_resume.pos.x = if isSimulating then -10000 else btn_simulate.width+8
			btn_cancel.width = (paramsRollout.width-14)
			btn_cancel.pos.x = if isSimulating then 8 else -10000
				
			spn_rate.pos.x = spn_emitEndTime.pos.x = spn_startTime.pos.x = spn_cacheStartTime.pos.x = spn_endTime.pos.x = spn_subSteps.pos.x =  spn_updateViewsEvery.pos.x = (paramsRollout.width-38)
			ddl_rateMode.width = (paramsRollout.width-14)
			ddl_rateMode.pos.x = 8
			
			btn_openBirthFlow.width = btn_openSimulationFlow.width = paramsRollout.width-38
			btn_openBirthFlow.pos.x = btn_openSimulationFlow.pos.x = 16

		)
		
		on paramsRollout open do 
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
			--btn_resume.visible = btn_simulate.visible = not isSimulating 
			--btn_cancel.visible = isSimulating 
			--prg_progress.visible = isSimulating 	
		)
		
		
	)
	
	rollout paramsLifespanRollout "Particle Lifespan"
	(
		checkbox chk_useLifeSpan "Delete 'Dead' Particles" offset:[-5,-4] across:2 tooltip:"When checked, particles with Age greater than the Lifespan will be deleted during the simulation.\n\nWhen unchecked, the Age and Lifespan will still be generated, but not used to delete particles during the simulation.\nYou could use a Magma and a Krakatoa Delete modifier to delete post-simulation."
		button btn_AgeLifespanPresets "^" width:18 height:18 align:#right offset:[10,-4] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,19,19,20,20) tooltip:"Click to add Krakatoa Magma and Krakatoa Delete modifiers to dynamically select and delete particles by Age and LifeSpan, or by NormalizedAge channel test."
		
		spinner spn_lifeSpan "Particle Lifespan " range:[0,1000000,25] type:#integer fieldwidth:36 offset:[60,-3] across:2 align:#right tooltip:"Defines the base value of the Lifespan.\n\nThis value is generated at Birth Time and is used to delete particles whose Age is greater than the LifeSpan when the 'Use Lifespan' checkbox is checked."
		button btn_lifeSpan_Preset ">" width:18 height:16 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the base value of the Lifespan.\n\nThis value is generated at Birth Time and is used to delete particles whose Age is greater than the LifeSpan when the 'Use Lifespan' checkbox is checked."
		
		spinner spn_lifeSpanVar "Lifespan Variation " range:[0,1000,5] type:#integer fieldwidth:36 offset:[60,-3] across:2 align:#right tooltip:"Defines the variation value of the Lifespan.\n\nThis value is used to produce a random variation of the Lifespan at Birth Time.\n\nIt is used as a +/- variation, in other words a Base Lifespan of 25 with a Variation of 5 will produce random values between 20 and 30."
		button btn_lifeSpanVar_Preset ">" width:18 height:16 align:#right offset:[10,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the variation value of the Lifespan.\n\nThis value is used to produce a random variation of the Lifespan at Birth Time.\n\nIt is used as a +/- variation, in other words a Base Lifespan of 25 with a Variation of 5 will produce random values between 20 and 30."
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do 
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)		
		
		fn updateButtonVisibility =
		(
			btn_AgeLifespanPresets.visible = FranticParticles != undefined
			btn_AgeLifespanPresets.enabled = selection.count > 0 and not ((for m in selection[1].modifiers where ((classof m == MagmaModifier and m.name == "Magma_SelectByAge") OR (classof m == KrakatoaDeleteModifier and m.name == "DeleteByAge")) collect m).count == 2)
		)
		
		on chk_useLifeSpan changed state do 
		(
			paramsRollout.updateSimAndResumeState()
			paramsRollout.updateUI()
		)
		on spn_lifeSpan changed val do 
		(
			paramsRollout.updateSimAndResumeState()
			paramsRollout.updateUI()
		)
		on spn_lifeSpanVar changed val do 
		(
			paramsRollout.updateSimAndResumeState()
			paramsRollout.updateUI()
		)
		
		fn addDeleteByAgeModifiers mode:#age =
		(
			try
			(
				local theName1 = if mode == #age then "Magma_SelectByAge" else "Magma_SelectByNormAge"
				local theName2 = if mode == #age then "DeleteByAge" else "DeleteByNormAge"
					
				local theMagma = MagmaModifier name:theName1
				modPanel.addModToSelection theMagma
				modPanel.addModToSelection (KrakatoaDeleteModifier name:theName2)
				modPanel.setCurrentObject selection[1].baseobject
				
				local magmaNode = theMagma.magmaHolder
				magmaNode.AutomaticRenameOFF = true
				
				node0 = magmaNode.createNode "Output" 
				magmaNode.setNodeProperty node0 "channelName" "Selection"
				magmaNode.setNodeProperty node0 "channelType" "float32"
				if mode == #age then
				(
					node1 = magmaNode.createNode "InputChannel" 
					magmaNode.setNodeProperty node1 "channelName" "Age"
					node2 = magmaNode.createNode "InputChannel" 
					magmaNode.setNodeProperty node2 "channelName" "LifeSpan"
				)
				else
				(
					node1 = magmaNode.createNode "InputChannel" 
					magmaNode.setNodeProperty node1 "channelName" "NormalizedAge"

					node2 = magmaNode.createNode "InputValue" 
					magmaNode.setNodeProperty node2 "forceInteger" false
					local ctrl=bezier_float(); ctrl.value = 1.0
					magmaNode.setNodeProperty node2 "controller" ctrl
					magmaNode.DeclareExtensionProperty node2 "Exposed"
					magmaNode.SetNodeProperty node2 "Exposed" true
					magmaNode.DeclareExtensionProperty node2 "Name"
					magmaNode.SetNodeProperty node2 "Name" "Age Threshold"					
				)

				node3 = magmaNode.createNode "Greater" 
				magmaNode.setNodeInputDefaultValue node3 2 0.0
				
				node4 = magmaNode.createNode "ToFloat" 
				magmaNode.setNodeInput node0 1 node4 1
				magmaNode.setNodeInput node3 1 node1 1
				magmaNode.setNodeInput node3 2 node2 1
				magmaNode.setNodeInput node4 1 node3 1
				
				local theNodes = #(node0,node1,node2,node3,node4)
				local thePositions = #([100,100], [470,30],[470,90], [610,20], [750,0])
				for i = 1 to theNodes.count do 
				(
					magmaNode.DeclareExtensionProperty theNodes[i] "Position"
					magmaNode.SetNodeProperty theNodes[i] "Position"  thePositions[i]
				)
				
				magmaNode.autoUpdate = true
				MagmaFlowEditor_Rollout = ::MagmaFlowEditor_Functions.OpenMagmaFlowEditor magmaNode offscreen:true
				MagmaFlowEditor_Rollout.exposeControlsToModifier()
				destroyDialog MagmaFlowEditor_Rollout
				
			)catch(messagebox "An error occurred while attempting to add Krakatoa Magma and Krakatoa Delete modifiers." title:"STOKE MX: Error")			
		)
		
		on btn_AgeLifespanPresets pressed do
		(
			popupMenu (defineRCAgeMenu()) pos:mouse.screenpos
		)

		on btn_lifeSpan_Preset pressed do popupPresetMenu #lifeSpan
		on btn_lifeSpan_Preset rightclick do popupPresetMenu #lifeSpan
		on btn_lifeSpanVar_Preset pressed do popupPresetMenu #lifeSpanVar
		on btn_lifeSpanVar_Preset rightclick do popupPresetMenu #lifeSpanVar
			
		fn updateLayout =
		(
			spn_lifeSpanVar.pos.x = spn_lifeSpan.pos.x = paramsLifespanRollout.width-34
			
		)			
			
		on paramsLifespanRollout open do
		(
			enableAutoLayout()
			updateLayout()
			updateButtonVisibility()
		)
	)	
	
	local dn_distribution_bg, dn_channels_bg, dn_velocity_bg, dn_velocity_fg, prtColor, surfaceColor, volumeColor, vertColor, edgeColor, grayColor, pflowColor, StokeFieldColor, forceColor, fumeColor
	local VectorColor, FloatColor, IntColor, VectorColor16, FloatColor16
	
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
			StokeFieldColor = (dotNetClass "System.Drawing.Color").fromARGB 150 0 0		
			forceColor = (dotNetClass "System.Drawing.Color").fromARGB 0 100 0

			VectorColor = (dotNetClass "System.Drawing.Color").fromARGB 120 0 0
			VectorColor16 = (dotNetClass "System.Drawing.Color").fromARGB 80 0 0
			FloatColor = (dotNetClass "System.Drawing.Color").fromARGB 0 120 0
			FloatColor16 = (dotNetClass "System.Drawing.Color").fromARGB 0 80 0
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
			StokeFieldColor =  (dotNetClass "System.Drawing.Color").fromARGB 255 100 100	
			forceColor = (dotNetClass "System.Drawing.Color").fromARGB 200 255 200

			VectorColor = (dotNetClass "System.Drawing.Color").fromARGB 255 200 200
			VectorColor16 = (dotNetClass "System.Drawing.Color").fromARGB 225 200 200
			FloatColor = (dotNetClass "System.Drawing.Color").fromARGB 200 255 200
			FloatColor16 = (dotNetClass "System.Drawing.Color").fromARGB 200 225 200
			IntColor = (dotNetClass "System.Drawing.Color").fromARGB 200 200 255					
		)			
	)	
	
	rollout paramsDistributionRollout "Distribution Sources"
	(
		fn filterPRTObjects obj =
		(
			--findItem GeometryClass.classes (classof obj) > 0 AND findItem distSources obj == 0 and obj != selection[1] and findItem #(targetObject, FumeFX) (classof obj.baseobject) == 0
			(StokeGlobalInterface.GetSourceType obj ) != #invalid and (allowMultiPickDistribution or findItem distSources obj == 0) and obj != selection[1] --and findItem #(targetObject, FumeFX) (classof obj.baseobject) == 0
		)
		checkbutton pck_pickObjects "Pick..." width:38 align:#left across:3 offset:[-12,-5] height:18 tooltip:"Pick a Distribution Object in the scene.\n\nSupported objects are all PRT objects including PRT Loader, PRT Volume, PRT Surface, PRT FumeFX, PRT Hair, PRT Maker and PRT Source."
		dotNetControl btn_addByName "Button" text:"By Name..." width:62 align:#center offset:[-2,-5] height:18 tooltip:"Pick Distribution Objects by name from a list."
		dotNetControl btn_removeSelectedSources "Button" text:"Remove" width:50 align:#right offset:[12,-5] height:18 tooltip:"Remove the highlighted object from the list."

		dotNetControl dnc_distributionSources "ListView" width:158 align:#center height:120 offset:[0,-4]

		spinner spn_perObjectJitter "Jitter Radius" type:#float range:[0,100000,10] fieldwidth:50 offset:[61,-3] across:2 align:#right tooltip:"Define the Random Jitter Radius .\n\nSpecifying 0 will disable the position randomization."
		button btn_perObjectJitter_Preset ">" width:18 height:16 align:#right offset:[11,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Define the Random Jitter Radius .\n\nSpecifying 0 will disable the position randomization."
		
		spinner spn_perObjectRate "Source Rate" type:#integer range:[0,10^8,1000] across:2 fieldwidth:50 offset:[61,-3] align:#right tooltip:"Defines the Emission Rate of the highlight distribution source objects."
		button btn_perObjectRate_Preset ">" width:18 height:16 align:#right offset:[11,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Emission Rate of the highlight distribution source objects."

		spinner spn_perObjectVolumeSpacing "Volume Spacing" type:#float range:[0.01,100000.0,1] across:2 fieldwidth:50 offset:[61,-3] align:#right tooltip:"Defines the Mesh Volume Spacing."
		button btn_perObjectVolumeSpacing_Preset ">" width:18 height:16 align:#right offset:[11,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Defines the Mesh Volume Spacing."
		
		checkbox chk_distUseNewParticlesOnly "Use New Particles Only" offset:[-10,-22] tooltip:"When checked, only newborn source particles will be considered as seeds for Stoke particles, assuming a valid ID channel.\n\nWhen unchecked, every particles will be used as seed regardless of its Age."
		checkbox chk_autoRate "Use Seed Count As Rate" offset:[-10,-3] align:#left tooltip:"When checked, the Rate will be controlled by the particle count of the source system. If a valid ID channel is provided and 'Use New Particles Only' is checked, this will recreate the source system exactly.\n\nIf checked and no ID channel can be found, or 'Use New Particles Only' is unchecked, every source particle will create one Stoke particle regardless of Age.\n\nIf unchecked, the Stoke Rate settings will be used."

		dropdownlist ddl_geometryMode items:#("Surface","Volume","Vertices","Edges") offset:[-10,-18] width:60 align:#left across:2 tooltip:"Defines the Mesh Distribution Mode."
		dropdownlist ddl_selectionMode items:#("All Faces","Face Selection","Vertex Soft Sel.") offset:[12,-18] width:98 align:#right tooltip:"Controls the Mesh Distribution use of the whole object, or its Face, Edge and Vertex Selections."
		
		dotNetControl dnc_availableChannels "ListView" width:158 align:#center height:136 offset:[0,-1]
		
		checkbox chk_allowMultiPickDistribution "Allow Multiple Object Picks" offset:[-10,-3] tooltip:"When checked, the same object can be picked more than once, for example so you can emit particles both from its Vertices and its Edges.\n\nWhen unchecked, each object can be picked only once as distribution source."
		checkbox chk_useViewportParticles "Emit From Viewport Particles" offset:[-10,-3] tooltip:"When unchecked, the render time particles of Distribution Sources will be used to seed simulation particles.\n\nWhen checked, the viewport particles will be used as seeds."

		checkbox chk_incrementRandomSeed "Vary Randomness Over Time" offset:[-10,-3] tooltip:"When checked, the Random Seed will be incremented by each frame.\n\nWhen unchecked, the same Random Seed will be used on every frame, unless explicitly keyframed."
		spinner spn_randomSeed "Random Seed " type:#integer range:[0,1000000,12345] fieldwidth:50 offset:[61,-2] across:2 tooltip:"Controls the random distribution particle selection.\n\nChanging this value will change the random pattern when picking particles to emit from."
		button btn_randomSeed_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Controls the random distribution particle selection.\n\nChanging this value will change the random pattern when picking particles to emit from."
		
		on chk_useViewportParticles changed state do paramsRollout.updateSimAndResumeState()
		on chk_incrementRandomSeed changed state do paramsRollout.updateSimAndResumeState()
		on spn_randomSeed changed val do paramsRollout.updateSimAndResumeState()
			
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)		
		
		fn getSourceChannelsList =
		(
			availableChannels = #()
			availableChannelNames = #("Velocity","ID","Age","LifeSpan","NormalizedAge")

			for theMagma in #(this.delegate.BirthMagma) do
			(
				for i in theMagma.GetNodes() do
				(
					if theMagma.getNodeType i == "Output" do 
					(
						local theChannel = theMagma.getNodeProperty i "channelName"
						local theChannelType = filterString (theMagma.getNodeProperty i "channelType") "[]"
						if theChannel != "" and findItem availableChannelNames theChannel == 0  do 
						(
							local formatType = "16"
							for i in channelsToSaveFormat where matchPattern i pattern:(theChannel+"*") do
							(
								if matchPattern i pattern:"*32*" do formatType = "32"
							)
							local theResult = ""
							theResult += case of
							(
								(matchPattern theChannelType[1] pattern:"float*"): ("float"+formatType)
								(matchPattern theChannelType[1] pattern:"int*"): ("int32")
								default: theChannelType[1]
							)
							if theChannelType[2] != undefined then
								theResult += "[" +theChannelType[2] as string + "]"
							else
								theResult += "[1]"						

							append availableChannels #(theChannel,theResult)
							append availableChannelNames theChannel
						)
					)
				)
			)			
			
			
			local activeDistSources = for i = 1 to distSources.count where isValidNode distSources[i] and distSourcesOnOff[i] != False collect distSources[i]
			for aSource in activeDistSources do
			(
				local pg = case ( StokeGlobalInterface.GetSourceType aSource ) of (
					#particles: ( StokeGlobalInterface.CreateKrakatoaGenerator aSource )
					#geometry: ( StokeGlobalInterface.CreateGeometryGenerator aSource )
					#fumefx: (StokeGlobalInterface.CreateFumeFXGenerator aSource )
					default: undefined
				)
				
				local theChannels = if pg != undefined then 
					try(for i in pg.AvailableChannels collect filterString i " []")catch(#())
				else
					#()
				tempArray = for aChannel in theChannels where findItem #("Position","Velocity","Age","LifeSpan","ID","NormalizedAge") aChannel[1] == 0 collect 
				(
					local formatType = "16"
					for i in channelsToSaveFormat where matchPattern i pattern:(aChannel[1]+"*") do
					(
						if matchPattern i pattern:"*32*" do formatType = "32"
					)
					local theResult = case of
					(
						(matchPattern aChannel[2] pattern:"float*"): ("float"+formatType)
						(matchPattern aChannel[2] pattern:"int*"): "int32"
						default: aChannel[2]
					)
					if aChannel[3] != undefined and aChannel[3] != 1 then
						theResult += "[" +aChannel[3] as string + "]"
					else
						theResult += "" --"[1]"
					#(aChannel[1],theResult)
				)
				for aChannel in tempArray where findItem availableChannelNames aChannel[1] == 0 do 
				(
					append availableChannelNames aChannel[1]
					append availableChannels aChannel
				)
			)
			local internalChannels = #(#("Velocity","float16[3]"),#("ID","int64"), #("Age","float16"),#("LifeSpan","float16"),#("NormalizedAge","float16"))
			for i in internalChannels do
			(		
				local theChannel = i[1]
				local theFormat = i[2]
				for j in channelsToSaveFormat where matchPattern j pattern:(theChannel+"*") do 
				(
					if matchPattern j pattern:"*32*" do theFormat = substituteString theFormat "16" "32"
				)
				append availableChannels #(theChannel, theFormat)
			)
			availableChannels
		)
		
		fn initDistListView =
		(
			layout_def = #( #("Source",90),  #("Rate",50), #("Emit From",60), #("Sub-Set",60),#("Jitter",60), #("Spacing",60))
			lv = dnc_distributionSources
			lv.Clear()
			lv.backColor = dn_distribution_bg
			lv.View = (dotNetClass "System.Windows.Forms.View").Details
			lv.gridLines = true 
			lv.fullRowSelect = true
			lv.checkboxes = true
			lv.hideSelection = false
			lv.ShowItemToolTips = true				
			lv.MultiSelect = false
			--showProperties lv
			for i in layout_def do lv.Columns.add i[1] i[2]
		)		
		
		fn initChannelsListView =
		(
			layout_def = #( #("Channel",100), #("Format",60) )
			lv = dnc_availableChannels
			lv.Clear()
			lv.backColor = dn_channels_bg
			lv.View = (dotNetClass "System.Windows.Forms.View").Details
			lv.gridLines = true 
			lv.fullRowSelect = true
			lv.checkboxes = true
			lv.hideSelection = false
			lv.ShowItemToolTips = true
			for i in layout_def do lv.Columns.add i[1] i[2]
		)


		
		fn updateObjectsList =
		(
			try
			(
				local theSel = getListViewSelection dnc_distributionSources
				local lv = dnc_distributionSources
				lv.items.Clear()
				local theRange = #()
				for i = 1 to distSources.count do
				(
					local theObj = distSources[i]
					theName = (if isValidNode theObj then theObj.name else "<Deleted>")
					local li = dotNetObject "System.Windows.Forms.ListViewItem" theName
					case (StokeGlobalInterface.GetSourceType DistSources[i]) of
					(
						#particles: (
							li.forecolor = prtColor
							li.tag = "prt"
						)
						#geometry:
						(
							li.forecolor = case geometryMode[i]  of
							(
								default: surfaceColor
								"Volume": volumeColor
								"Vertices": vertColor
								"Edges": edgeColor
							)
							li.tag = "mesh"
						)
						#FumeFX:
						(
							li.forecolor = fumeColor
							li.tag = "fume"
						)
						default:
						(
							li.forecolor = grayColor
							li.tag = "none"
						)
					)
					li.checked = DistSourcesOnOff[i] != False and isValidNode theObj 
					local subLi = li.SubItems.add (distRates[i] as string)
					local subLi = li.SubItems.add ""
					local subLi = li.SubItems.add ""
					local subLi = li.SubItems.add ""
					local subLi = li.SubItems.add ""
					append theRange li
				)
				lv.Items.AddRange theRange
				paramsDistributionRollout.refreshRateValues()
				setListViewSelection dnc_distributionSources theSel						
			)catch(format "--Error Updating Stoke Distribution Objects List\n")
		)
		
		fn refreshRateValues =
		(
			local totalActiveNodes = for o = 1 to DistSources.count where DistSourcesOnOff[o] != False collect o
			if rateMode == 1 then
			(
				local totalRates = 0.0
				local activeEmittersCount = 0
				for o = 1 to DistSources.count where isValidNode DistSources[o] and DistSourcesOnOff[o] != False do 
				(
					totalRates += distRates[o]
					activeEmittersCount += 1
				)
				try(dnc_distributionSources.columns.item[1].Text = "Ratio")catch()
			)
			else
			(
				try(dnc_distributionSources.columns.item[1].Text = "Rate")catch()
			)
			--btn_perObjectRate_Preset.enabled = spn_perObjectRate.enabled = findItem #(1,4) rateMode > 0 and not chk_autoRate.state
			--btn_perObjectRate_Preset.visible = spn_perObjectRate.visible = rateMode != 1
			
			for i = 1 to distSources.count do
			(
				local theValue = if distSeedAsRate[i] == true then
				(
					"Auto"
				)
				else
				(
					case rateMode of
					(
						1: (
							if totalRates == 0 or distSourcesOnOff[i] == False then 
								"0.0%"							
							else 
								(((floor ((distRates[i] as float)/totalRates*1000))/10.0) ) as string + "%" 
							
						)
						2: (((floor ((1.0*rate/totalActiveNodes.count)+0.5)) as integer )as string)
						3: (rate as string)
						default: (distRates[i] as string)
					)
				)
				
				try(dnc_distributionSources.items.item[i-1].subItems.Item[1].Text = theValue)catch()
				
		
				local theJRString = if classof distJitterRadius[i] != Float then "1.0" else distJitterRadius[i] as string
				local theVSString = if classof distVolumeSpacing[i] != Float then "1.0" else distVolumeSpacing[i] as string

				local isMesh = true
				local theModeString = geometryMode[i] 
				if theModeString == undefined do theModeString = "Surface"
					
				local theSelString = case distSourcesSelectionType[i] of
				(
					default: ("All ")
					"VertexSelection": "Vertex Soft Selection"
					"FaceSelection": 
					(
						case theModeString of
						(
							"Edges": "Edge Selection" 
							"Surface": "Face Selection"
							default: "All"
						)
					)
				)				
				
				try
				(
					isMesh = dnc_distributionSources.items.item[i-1].tag == "mesh"
					if not isMesh do 
						theModeString = if dnc_distributionSources.items.item[i-1].tag == "fume" then "Source Voxels" else 
							if distUseNewParticlesOnly[i] != False then "New Part." else "Particles"
					local theTooltipText = (if isValidNode distSources[i] then distSources[i].name else "<Deleted>") + "\n\n"
					
					theTooltipText += "Emit From: " + theModeString  
					if theModeString == "Volume" then 
						theTooltipText += ", Spacing: " + theVSString  + "\n"
					else
						theTooltipText += "\n"
					if isMesh do theTooltipText += "Sub-Set: " + theSelString + "\n"
					theTooltipText += "Rate: " 
					theTooltipText += if distSeedAsRate[i] == true then "Auto" else (distRates[i] as string + (if rateMode == 1 then (" ("+theValue+")") else "" ))
					theTooltipText += "\n"
					theTooltipText += "Jitter: " + theJRString  + "\n"
						
					dnc_distributionSources.items.item[i-1].TooltipText = theTooltipText
				)catch()
				try(dnc_distributionSources.items.item[i-1].subItems.Item[2].Text = theModeString )catch()				
				local theSelString = case distSourcesSelectionType[i] of
				(
					default: "All"
					"VertexSelection": 
					(
						if theModeString == "Volume" then "All" else "Vert SS"
					)
					"FaceSelection": 
					(
						case theModeString of 
						(
							"Edges": "Edge Sel." 
							"Surface": "Face Sel."
							default: "All"
						)
					)
				)	
				try
				(
					dnc_distributionSources.items.item[i-1].subItems.Item[3].Text = theSelString
					
					if isMesh then 
					(
						dnc_distributionSources.items.item[i-1].forecolor = case geometryMode[i]  of
						(
							default: surfaceColor
							"Volume": volumeColor
							"Vertices": vertColor
							"Edges": edgeColor
						)				
					)
					else
					(
						--dnc_distributionSources.items.item[i-1].forecolor = prtColor
					)
				)catch()	
				local theVal = distJitterRadius[i]
				if theVal == undefined do theVal = 10.0
				try(dnc_distributionSources.items.item[i-1].subItems.Item[4].Text = theVal as string)catch()

				local theVal = distVolumeSpacing[i]
				if theVal == undefined do theVal = 1.0
				if theVal < 0.01 do theVal = "(0.01)"
				if not isMesh do theVal = "N/A"
				try(dnc_distributionSources.items.item[i-1].subItems.Item[5].Text = theVal as string)catch()
				
			)--end i loop
		)

		fn updateChannelsList =
		(
			local lv = dnc_availableChannels
			lv.items.Clear()
			local theRange = #()
			for aChannel in availableChannels do
			(
				local li = dotNetObject "System.Windows.Forms.ListViewItem" aChannel[1]
				li.tooltiptext = (aChannel[1] + " " + aChannel[2])
				li.checked = findItem channelsToSave aChannel[1] > 0
				li.forecolor = case of
				(
					default: IntColor
					(matchPattern aChannel[2] pattern:"float16[3]"): VectorColor16
					(matchPattern aChannel[2] pattern:"float32[3]"): VectorColor 
					(matchPattern aChannel[2] pattern:"float16"): FloatColor16
					(matchPattern aChannel[2] pattern:"float32"): FloatColor
				)
				local subLi = li.SubItems.add aChannel[2]
				append theRange li
			)
			lv.Items.AddRange theRange 					
		)

		fn updateControlVisibility = 
		(
			local isInCreateMode = getCommandPanelTaskMode() == #create
			spn_perObjectRate.enabled = btn_perObjectRate_Preset.enabled = spn_perObjectJitter.enabled = btn_perObjectJitter_Preset.enabled = not isInCreateMode
			pck_pickObjects.enabled = btn_addByName.enabled = btn_removeSelectedSources.enabled = spn_perObjectVolumeSpacing.enabled = btn_perObjectVolumeSpacing_Preset.enabled = ddl_geometryMode.enabled = ddl_selectionMode.enabled = not isInCreateMode
			
			local isMesh = false
			local isParticles = false
			local theSel = getListViewSelection dnc_distributionSources
			if theSel.count > 0 then
			(
				isMesh = dnc_distributionSources.items.item[theSel[1]-1].tag == "mesh"
				isParticles = not isMesh
				spn_perObjectRate.visible = btn_perObjectRate_Preset.visible = spn_perObjectJitter.visible = btn_perObjectJitter_Preset.visible = true
				spn_perObjectVolumeSpacing.visible = btn_perObjectVolumeSpacing_Preset.visible = ddl_geometryMode.visible = ddl_selectionMode.visible = isMesh
				if isMesh then
				(
					dnc_availableChannels.pos = [2,218]
					dnc_availableChannels.height = 140
				)
				else
				(
					dnc_availableChannels.pos = [2,210]
					dnc_availableChannels.height = 148
				)
				chk_distUseNewParticlesOnly.visible = chk_autoRate.visible = isParticles
			)
			else
			(
				chk_distUseNewParticlesOnly.visible = ddl_selectionMode.visible = ddl_geometryMode.visible = chk_autoRate.visible = spn_perObjectVolumeSpacing.visible = btn_perObjectVolumeSpacing_Preset.visible = false
				spn_perObjectRate.visible = btn_perObjectRate_Preset.visible = spn_perObjectJitter.visible = btn_perObjectJitter_Preset.visible = false
				dnc_availableChannels.pos = [2,140]
				dnc_availableChannels.height = 218
			)
			
		)
		
		fn updateUI =
		(
			getSourceChannelsList()
			updateControlVisibility()
		)
		
		on btn_randomSeed_Preset pressed do popupPresetMenu #randomSeed
		on btn_randomSeed_Preset rightclick do popupPresetMenu #randomSeed
		
		on btn_JitterRadius_Preset pressed do popupPresetMenu #JitterRadius
		on btn_JitterRadius_Preset rightclick do popupPresetMenu #JitterRadius
			
		on btn_perObjectRate_Preset pressed do popupPresetMenu #perObjectRate
		on btn_perObjectRate_Preset rightclick do popupPresetMenu #perObjectRate
			
		on btn_perObjectJitter_Preset pressed do popupPresetMenu #perObjectJitter
		on btn_perObjectJitter_Preset rightclick do popupPresetMenu #perObjectJitter
			
		on btn_perObjectVolumeSpacing_Preset pressed do popupPresetMenu #perObjectVolumeSpacing
		on btn_perObjectVolumeSpacing_Preset rightclick do popupPresetMenu #perObjectVolumeSpacing
			
		fn updatePerObjectRate =
		(
			if not lockUpdates do 
			(
				local theSel = getListViewSelection dnc_distributionSources
				for i in theSel do
				(
					distRates[i] = perObjectRate
				)
				refreshRateValues()
				setListViewSelection dnc_distributionSources theSel			
			)
		)
			
		on spn_perObjectRate changed val do
		(
			updatePerObjectRate()
		)
		
		fn updatePerObjectSpacing =
		(
			if not lockUpdates do 
			(
				local theSel = getListViewSelection dnc_distributionSources
				for i in theSel do
				(
					distVolumeSpacing[i] = perObjectVolumeSpacing
				)
				refreshRateValues()
				setListViewSelection dnc_distributionSources theSel			
			)
		)
		
		on spn_perObjectVolumeSpacing changed val do 
		(
			updatePerObjectSpacing()
		)
		
		fn updateperObjectJitter =
		(
			if not lockUpdates do 
			(
				local theSel = getListViewSelection dnc_distributionSources
				for i in theSel do
				(
					distJitterRadius[i] = perObjectJitter
				)
				refreshRateValues()
				setListViewSelection dnc_distributionSources theSel			
			)			
		)
		
		fn updatePerObjectControls =
		(
			ddl_geometryMode.enabled = btn_perObjectRate_Preset.enabled = spn_perObjectRate.enabled = ddl_selectionMode.enabled = chk_distUseNewParticlesOnly.enabled = chk_autoRate.enabled = false
			lockUpdates = true
			local theSel = getListViewSelection dnc_distributionSources
			if theSel.count > 0 do 
			(
				perObjectRate = distRates[theSel[1]]
				theVal = distJitterRadius[theSel[1]]
				if theVal == undefined do theVal =10.0
				perObjectJitter = theVal
				
				theVal = distVolumeSpacing[theSel[1]]
				if theVal == undefined do theVal =1.0
				perObjectVolumeSpacing = theVal
				
				theIndex = findItem #("Surface","Volume","Vertices","Edges") geometryMode[theSel[1]]
				if theIndex == 0 do theIndex = 1
				ddl_geometryMode.selection = theIndex	
				
				case theIndex of
				(
					default: (
						ddl_selectionMode.items = #("All Faces","Vertex Soft Sel.","Face Selection")
						theIndex2 = findItem #("","VertexSelection","FaceSelection") distSourcesSelectionType[theSel[1]]
					)
					2: (
						ddl_selectionMode.items = #("All")
						theIndex2 = 1
					)
					3: (
						ddl_selectionMode.items = #("All","Vertex Soft Sel.")
						theIndex2 = findItem #("","VertexSelection") distSourcesSelectionType[theSel[1]]
					)
					4: (
						ddl_selectionMode.items = #("All","Vertex Soft Sel.","Edge Selection")
						theIndex2 = findItem #("","VertexSelection","FaceSelection") distSourcesSelectionType[theSel[1]]
					)
				)
				if theIndex2 == 0 do theIndex2 = 1
				ddl_selectionMode.selection = theIndex2
				
				
				ddl_geometryMode.enabled = ddl_selectionMode.enabled = dnc_distributionSources.items.item[theSel[1]-1].Tag == "mesh"
				
				
				chk_distUseNewParticlesOnly.enabled = chk_autoRate.enabled = dnc_distributionSources.items.item[theSel[1]-1].Tag == "prt"
				chk_autoRate.state = distSeedAsRate[theSel[1]] == true
				chk_distUseNewParticlesOnly.state = distUseNewParticlesOnly[theSel[1]] != False
				btn_perObjectRate_Preset.enabled = spn_perObjectRate.enabled = findItem #(1,4) rateMode > 0 AND not chk_autoRate.state
			)
			lockUpdates = false
		)		
		
		on spn_perObjectJitter changed val do
		(
			updateperObjectJitter()
		)
		
		on chk_autoRate changed state do
		(
			if not lockUpdates do 
			(
				local theSel = getListViewSelection dnc_distributionSources
				for i in theSel do
				(
					distSeedAsRate[i] = state
				)
				refreshRateValues()
				updatePerObjectControls()
				setListViewSelection dnc_distributionSources theSel			
			)			
		)		
		
		on chk_distUseNewParticlesOnly  changed state do
		(
			if not lockUpdates do 
			(
				local theSel = getListViewSelection dnc_distributionSources
				for i in theSel do
				(
					distUseNewParticlesOnly[i] = state
				)
				refreshRateValues()
				updatePerObjectControls()
				setListViewSelection dnc_distributionSources theSel			
			)			
		)		
		
		on dnc_distributionSources mouseClick arg do 
		(
			local theSel = getListViewSelection dnc_distributionSources
			if theSel.count > 0 and arg.Button == arg.Button.Right do
			(
				RCMenuSourceObject = distSources[theSel[1]]
				local theMenu = defineRCMenu()
				popupMenu theMenu pos:mouse.screenpos
			)
		)
		
		on dnc_distributionSources mouseUp arg do 
		(
			updateControlVisibility()
			updatePerObjectControls()
		)
		
		on dnc_distributionSources ItemChecked arg do
		(
			local newArray = #()
			for i = 0 to dnc_distributionSources.items.count-1 do
			(
				newArray[i+1] = try(dnc_distributionSources.items.item[i].checked)catch(False)
			)
			DistSourcesOnOff = newArray
			paramsRollout.updateUI()
			getSourceChannelsList()
			updateChannelsList()
			updateControlVisibility()
			refreshRateValues()
			
		)		
		
		on dnc_availableChannels ItemChecked arg do
		(
			local newArray = #()
			for i = 0 to dnc_availableChannels.items.count-1 do
			(
				try(if dnc_availableChannels.items.item[i].checked do appendIfUnique newArray dnc_availableChannels.items.item[i].Text)catch()
			)
			channelsToSave = newArray
		)
		
		fn selectAllChannels =
		(
			local theSel = for i = 1 to dnc_availableChannels.items.count collect i
			setListViewSelection dnc_availableChannels theSel						
		)
		
		fn InvertSelectedlChannels =
		(
			local theSel = getListViewSelection dnc_availableChannels
			theSel = for i = 1 to dnc_availableChannels.items.count where findItem theSel i == 0 collect i
			setListViewSelection dnc_availableChannels theSel						
		)
		
		fn selectChannelsByType type:32 vector:false =
		(
			local theSel = if vector then
				for i = 1 to dnc_availableChannels.items.count where matchPattern dnc_availableChannels.items.item[i-1].subItems.Item[1].Text pattern:("float"+type as string+"[3]") collect i
			else
				for i = 1 to dnc_availableChannels.items.count where matchPattern dnc_availableChannels.items.item[i-1].subItems.Item[1].Text pattern:("float"+type as string) collect i
			setListViewSelection dnc_availableChannels theSel	
		)
		
		fn switchChannelTypeTo type:32 =
		(
			local theSel = getListViewSelection dnc_availableChannels
			for i in theSel do
			(
				for j = channelsToSaveFormat.count to 1 by -1 where matchPattern channelsToSaveFormat[j] pattern:(dnc_availableChannels.items.item[i-1].Text+"*") do
					deleteItem channelsToSaveFormat j
				if type == 32 do append channelsToSaveFormat (dnc_availableChannels.items.item[i-1].Text+" 32")
			)
			getSourceChannelsList()
			updateChannelsList()
		)
		
		on dnc_availableChannels mouseClick arg do
		(
			local theSel = getListViewSelection dnc_availableChannels
			if theSel.count > 0 and arg.Button == arg.Button.Right do
			(
				local theMenu = defineChannelTypeRCMenu()	
				popupMenu theMenu pos:mouse.screenpos
			)
		)		
		
		on ddl_selectionMode selected itm do
		(
			local theSel = getListViewSelection dnc_distributionSources
			for i in theSel do
			(
				case geometryMode[i] of
				(
					default: distSourcesSelectionType[i] = #("","VertexSelection","FaceSelection")[itm]
					2: distSourcesSelectionType[i] = 1 
					3: distSourcesSelectionType[i] = #("","VertexSelection")[itm]
				)
			)
			refreshRateValues()	
		)
		
		on ddl_geometryMode selected itm do
		(
			local theSel = getListViewSelection dnc_distributionSources
			for i in theSel do
			(
				geometryMode[i] = #("Surface","Volume","Vertices","Edges")[itm]
			)
			refreshRateValues()		
			updatePerObjectControls()				
		)

		
		
		on pck_pickObjects changed state do--picked obj do 
		(
			if state then
			(
				obj = pickObject filter:filterPRTObjects
				if obj != undefined do
				(
					local theType = (StokeGlobalInterface.GetSourceType obj )
					if theType != #invalid do
					(
						if classof obj == PF_Source then
						(
							global Magma_SelectedParticleGroup = #()
							::StokeFieldMFEditor_Functions.getEventFromPFSource obj multiple:true
							if ::Magma_SelectedParticleGroup  != undefined do
							(
								for aGroup in ::Magma_SelectedParticleGroup where findItem distSources aGroup == 0 do
								(
									append distSources aGroup
									distSourcesOnOff[distSources.count] = True
									append distRates perObjectRate
									append geometryMode ""
									append distSourcesSelectionType ""
									append distJitterRadius perObjectJitter		
									append distVolumeSpacing 1.0	
									append distSeedAsRate false
									append distUseNewParticlesOnly true
								)
							)
						)
						else
						(
							append distSources obj
							distSourcesOnOff[distSources.count] = True
							append distRates perObjectRate
							append geometryMode ""
							append distSourcesSelectionType ""
							append distSeedAsRate false
							append distUseNewParticlesOnly true
						
							if theType == #geometry then
							(
								append distJitterRadius 0.0
								local theBBox = obj.max-obj.min
								local theMaxSize = amax #(theBBox.x, theBBox.y, theBBox.z)
								append distVolumeSpacing (theMaxSize/50.0)						
							)
							else
							(
								append distJitterRadius 0.0
								append distVolumeSpacing 1.0
							)
						)

						updateObjectsList()
						getSourceChannelsList()
						updateChannelsList()
					)
				)
			)
			pck_pickObjects.state = false
		)
		
		on btn_addByName click arg do
		(
			theNodes = selectByName filter:filterPRTObjects multiple:true showHidden:true
			if theNodes != undefined do
			(
				for obj in theNodes do 
				(
					local theType = (StokeGlobalInterface.GetSourceType obj)
					if theType != #invalid do
					(
						if classof obj == PF_Source then
						(
							global Magma_SelectedParticleGroup = #()
							::StokeFieldMFEditor_Functions.getEventFromPFSource obj multiple:true
							if ::Magma_SelectedParticleGroup  != undefined do
							(
								for aGroup in ::Magma_SelectedParticleGroup where findItem distSources aGroup == 0 do
								(							
									append distSources aGroup
									distSourcesOnOff[distSources.count] = True
									append distRates perObjectRate
									append geometryMode ""
									append distSourcesSelectionType ""
									append distJitterRadius perObjectJitter		
									append distVolumeSpacing	0.0
								)
							)
						)		
						else						
						(
							append distSources obj
							distSourcesOnOff[distSources.count] = True
							append distRates perObjectRate
							append geometryMode ""
							append distSourcesSelectionType ""
							if theType == #geometry then
								append distJitterRadius 0.0
							else
								append distJitterRadius perObjectJitter					

							local theBBox = obj.max-obj.min
							local theMaxSize = amax #(theBBox.x, theBBox.y, theBBox.z)
							append distVolumeSpacing (theMaxSize/50.0)
						)
					)
				)
				updateObjectsList()
				getSourceChannelsList()
				updateChannelsList()
			)
		)
		
		on btn_removeSelectedSources click arg do
		(
			local theSel = getListViewSelection dnc_distributionSources
			if theSel.count > 0 do
			(
				deleteItem distSources theSel[1]
				try(deleteItem distSourcesOnOff theSel[1])catch()
				try(deleteItem distRates theSel[1])catch()
				try(deleteItem geometryMode theSel[1])catch()
				try(deleteItem distSourcesSelectionType theSel[1])catch()
				try(deleteItem distJitterRadius theSel[1])catch()
				try(deleteItem distVolumeSpacing theSel[1])catch()
				try(deleteItem distSeedAsRate theSel[1])catch()
				try(deleteItem distUseNewParticlesOnly theSel[1])catch()
				
				updateObjectsList()
				getSourceChannelsList()
				updateChannelsList()
				paramsRollout.updateUI()
				
				if theSel[1] > dnc_distributionSources.Items.count do theSel[1] -= 1
				setListViewSelection dnc_distributionSources theSel[1]
			)
		)	
		
		on rad_distributionMode changed state do updateUI()
		
		fn updateObjectsListAnimation =
		(
			refreshRateValues()
		)
		
		fn updateLayout =
		(
			btn_addByName.FlatStyle = btn_removeSelectedSources.FlatStyle = btn_addByName.FlatStyle.System
			btn_addByName.width = btn_removeSelectedSources.width = (paramsDistributionRollout.width-42)/2
			btn_addByName.pos.x = 40
			btn_removeSelectedSources.pos.x = btn_addByName.pos.x + btn_addByName.width + 1
			dnc_availableChannels.width = dnc_distributionSources.width = paramsDistributionRollout.width-4
			dnc_availableChannels.pos.x = dnc_distributionSources.pos.x = 2
			
			spn_randomSeed.pos.x = spn_perObjectJitter.pos.x = spn_perObjectRate.pos.x = spn_perObjectVolumeSpacing.pos.x =  (paramsDistributionRollout.width-32)
			
			ddl_selectionMode.width = ddl_geometryMode.width = (paramsDistributionRollout.width-8)/2
			ddl_geometryMode.pos.x = 4
			ddl_selectionMode.pos.x = ddl_geometryMode.width+4
		)		
		
		on paramsDistributionRollout open do 
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
			getUIColors()
			initDistListView()
			updateObjectsList()
			
			initChannelsListView()
			updateChannelsList()
			
			updatePerObjectControls()
		)
	)	

	rollout paramsVelocityFieldRollout "Velocity Field Sources"
	(
		fn filterSources obj = (findItem VelocitySources obj == 0 or (not useGlobalGrid and (StokeGlobalInterface.GetVelocityType obj ) == #particles)) and (WSMSupportsForce obj or (StokeGlobalInterface.GetVelocityType obj ) != #invalid) and obj != selection[1]
		checkbutton pck_pickObjects "Pick..." width:38 align:#left across:3 offset:[-12,-5] height:18 tooltip:"Pick a Velocity Field Source from the scene.\n\nValid objects are Max Force SpaceWarps, FumeFX Simulations and Stoke Field objects."
		dotNetControl btn_addByName "Button" text:"By Name..." width:62 align:#center offset:[-2,-5] height:18 tooltip:"Pick Velocity Field Source by name from a list."
		dotNetControl btn_removeSelectedSources "Button" text:"Remove" width:50 align:#right offset:[12,-5] height:18 tooltip:"Remove the highlighted object from the list."

		dotNetControl dnc_VelocitySources "ListView" width:158 align:#center height:120 offset:[0,-4]			
		
		spinner spn_VelocityScale "Velocity Scale:" range:[-10000,10000,1.0] fieldwidth:50 enabled:true across:2 offset:[60,-2] tooltip:"Scale the Velocity before applying it to the simulation.\n\nA value of 1.0 will use the Velocity as read from the source object."
		button btn_VelocityScale_Preset ">" width:18 height:16 align:#right offset:[10,-2] enabled:true images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Scale the Velocity before applying it to the simulation.\n\nA value of 1.0 will use the Velocity as read from the source object."

		group "Velocity From Particles" 	
		(
			checkbox chk_useGlobalGrid "Use Global Grid Settings" align:#left offset:[-5,-3] tooltip:"When checked, all Particle-based Velocity Sources will share the same Grid Spacing, Padding and Create Fluid Motion settings.\n\nWhen unchecked, each Particle-based Velocity Source will have its own per-object Grid settings, and you will be allowed to pick the same Particle multiple times, for example to add two different Grids of the same Source together."
			spinner spn_gridSize "Grid Spacing:" range:[0.1,100000.0,10.0] fieldWidth:45 enabled:true offset:[55,-2] across:2 type:#worldunits tooltip:"Set the Voxel Size in the Grid used to convert particle velocities to a Velocity Field.\n\nThe Grid dimensions will be set adaptively based on the bounding box of the particle system."
			button btn_gridSize_Preset ">" width:18 height:16 align:#right offset:[5,-2] enabled:true images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Voxel Size in the Grid used to convert particle velocities to a Velocity Field.\n\nThe Grid dimensions will be set adaptively based on the bounding box of the particle system."

			spinner spn_gridPadding "Grid Padding:" range:[1,100,5] fieldWidth:45 enabled:true offset:[55,-3] across:2 type:#integer tooltip:"Set the Number of Voxels to add to each side of the Adaptive Grid used to convert particle Velocities to a Velocity Field."
			button btn_gridPadding_Preset ">" width:18 height:16 align:#right offset:[5,-3] enabled:true images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Number of Voxels to add to each side of the Adaptive Grid used to convert particle Velocities to a Velocity Field."
			
			checkbox chk_fluidMotion "Create Fluid Motion" offset:[-5,-3]
			--checkbox chk_useViewportParticlesField "Use Viewport Particles" offset:[-5,-3]
			
		)

		on spn_gridSize changed val do paramsRollout.updateSimAndResumeState()
		on spn_gridPadding changed val do paramsRollout.updateSimAndResumeState()
		on chk_fluidMotion changed state do paramsRollout.updateSimAndResumeState()
		--on chk_useViewportParticlesField changed state do paramsRollout.updateSimAndResumeState()
		
		fn initDistListView =
		(
			getUIColors()
			layout_def = #( #("Velocity Source",105), #("Scale",47), #("Grid",50),#("Padding",50),#("Fluid",50))
			lv = dnc_VelocitySources
			lv.Clear()
			lv.backColor = dn_velocity_bg
			lv.View = (dotNetClass "System.Windows.Forms.View").Details
			lv.gridLines = true 
			lv.fullRowSelect = true
			lv.checkboxes = true
			lv.hideSelection = false
			lv.ShowItemToolTips = true			
			lv.multiSelect = false
			for i in layout_def do lv.Columns.add i[1] i[2]
		)			
		
		fn updateVelocityList =
		(
			--try(
				local theSel = getListViewSelection dnc_VelocitySources
				local lv = dnc_VelocitySources
				lv.items.Clear()
				local theRange = #()
				for i = 1 to VelocitySources.count do
				(
					local theObj = VelocitySources[i]
					local theName = (if isValidNode theObj then theObj.name else "<Deleted>")
					local li = dotNetObject "System.Windows.Forms.ListViewItem" theName
					local theTooltipText = theName + "\n"
					theTooltipText += "Velocity Scale: " + (velocityScales[i] as string)+"\n\n"
					if isValidNode theObj do
					(
						if StokeGlobalInterface.GetVelocityType theObj == #particles do
						(
							local theGridSize = gridSizes[i]
							if theGridSize == undefined or useGlobalGrid do theGridSize = gridSize
							local theGridPadding = gridPaddings[i]
							if theGridPadding  == undefined or useGlobalGrid do theGridPadding  = GridPadding 
							local theFluidMotion = fluidMotions[i]
							if theFluidMotion  == undefined or useGlobalGrid do theFluidMotion  = FluidMotion 
								
							local theGridPrefix = if useGlobalGrid then "Global " else "Object "
							theTooltipText += theGridPrefix + "Grid Spacing: " + (theGridSize as string)+"\n"
							theTooltipText += theGridPrefix + "Grid Padding: " + (theGridPadding  as string)+"\n"
							theTooltipText += theGridPrefix + "Fluid Motion: " + (if theFluidMotion then "Yes" else "No")+"\n\n"
						)
						local thePropNames = case classof theObj.baseobject of
						(				
							StokeFieldSim: (for p in getPropNames theObj.baseobject where findItem #(#holder, #object) p == 0 collect p)
							Stoke_Field: (for p in getPropNames theObj.baseobject where findItem #(#Stoke_Field_Static) p == 0 collect p)
							FumeFX: #(#GridSpacing, #Width, #Length, #Height, #velocityScale, #StartFrame, #EndFrame, #PlayFrom, #Playto, #Offset)
							Thinking: #()
							PHXSimulator: #() -- avoid "Couldn't find a previously defined paramblock" error with PhoenixFD 2.2
							default: if isValidNode theObj then getPropNames theObj.baseobject else #()
						)
						for p in thePropNames do
						(
							theTooltipText += p as string + ": " + (getProperty theObj.baseobject p) as string + "\n"
						)
					)--end is valid node
					li.tooltiptext = theTooltipText
					li.checked = isValidNode theObj and VelocitySourcesOnOff[i] != false
					li.forecolor = if isValidNode theObj then
					(
						case of
						(
							(WSMSupportsForce theObj): forceColor
							(classof theObj.baseobject == ParticleGroup): pflowColor
							(classof theObj.baseobject == fumeFX): fumeColor
							(classof theObj.baseobject == StokeFieldSim): StokeFieldColor
							(classof theObj.baseobject == Stoke_Field): StokeFieldColor
							(StokeGlobalInterface.GetVelocityType theObj == #particles): prtColor
							default: dn_velocity_fg
						)
					)
					else grayColor
					
					local subLi = li.SubItems.add (velocityScales[i] as string)
					if isValidNode theObj and StokeGlobalInterface.GetVelocityType theObj == #particles then
					(
						local subLi = li.SubItems.add (theGridSize as string)
						local subLi = li.SubItems.add (theGridPadding as string)
						local subLi = li.SubItems.add (if theFluidMotion then "Yes" else "No")
					)
					else
					(
						local subLi = li.SubItems.add ("N/A")
						local subLi = li.SubItems.add ("N/A")
						local subLi = li.SubItems.add ("N/A")
					)
					append theRange li
				)
				lv.Items.AddRange theRange 			
				setListViewSelection dnc_VelocitySources theSel						
			--)catch()
		)
		
		fn refreshScaleValues =
		(
			for i = 1 to VelocitySources.count do
			(
				try(dnc_VelocitySources.items.item[i-1].subItems.Item[1].Text = (velocityScales[i] as string))catch()
			)
		)
		
		fn refreshGridValues =
		(
			for i = 1 to VelocitySources.count do
			(
				if StokeGlobalInterface.GetVelocityType velocitySources[i] == #particles do
				(
					local theGridSize = gridSizes[i]
					if theGridSize == undefined or useGlobalGrid do theGridSize = gridSize
					local theGridPadding = gridPaddings[i]
					if theGridPadding  == undefined or useGlobalGrid do theGridPadding  = GridPadding 
					local theFluidMotion = fluidMotions[i]
					if theFluidMotion  == undefined or useGlobalGrid do theFluidMotion  = FluidMotion 
					try(dnc_VelocitySources.items.item[i-1].subItems.Item[2].Text = (theGridSize as string))catch()
					try(dnc_VelocitySources.items.item[i-1].subItems.Item[3].Text = (theGridPadding as string))catch()
					try(dnc_VelocitySources.items.item[i-1].subItems.Item[4].Text = (if theFluidMotion then "Yes" else "No"))catch()
				)
			)
		)

		fn updatePerObjectScale =
		(
			local theSel = getListViewSelection dnc_VelocitySources
			for i in theSel do
			(
				velocityScales[i] = velocityScale
			)
			refreshScaleValues()
		)
		
		fn updateEnabledStates =
		(
			local theSel = getListViewSelection dnc_VelocitySources
			btn_VelocityScale_Preset.enabled = spn_VelocityScale.enabled = theSel.count > 0
			btn_gridSize_Preset.enabled = btn_gridPadding_Preset.enabled = chk_fluidMotion.enabled = spn_gridPadding.enabled = spn_gridSize.enabled = useGlobalGrid or (theSel.count > 0 and (StokeGlobalInterface.GetVelocityType VelocitySources[theSel[1]] == #particles))
			if theSel.count > 0 then
			(
				spn_gridSize.value = if useGlobalGrid or gridSizes[theSel[1]] == undefined then gridSize else gridSizes[theSel[1]]
				spn_gridPadding.value = if useGlobalGrid or gridPaddings[theSel[1]] == undefined then gridPadding else gridPaddings[theSel[1]] 
				chk_fluidMotion.state = if useGlobalGrid or fluidMotions[theSel[1]] == undefined then fluidMotion else fluidMotions[theSel[1]]  
			)
			else
			(
				spn_gridSize.value = gridSize
				spn_gridPadding.value = gridPadding
				chk_fluidMotion.state = fluidMotion
			)
		)		
			
		on spn_VelocityScale changed val do
		(
			updatePerObjectScale()
		)		
		
		on spn_gridSize changed val do
		(
			local theSel = getListViewSelection dnc_VelocitySources
			if useGlobalGrid then 
				gridSize = val
			else
				gridSizes[theSel[1]] = val
			refreshGridValues()
		)
		on spn_gridPadding changed val do
		(
			local theSel = getListViewSelection dnc_VelocitySources
			if useGlobalGrid then 
				gridPadding = val
			else
				gridPaddings[theSel[1]] = val
			refreshGridValues()
		)	
		on chk_fluidMotion changed val do
		(
			local theSel = getListViewSelection dnc_VelocitySources
			if useGlobalGrid then 
				fluidMotion = val
			else
				fluidMotions[theSel[1]] = val
			refreshGridValues()			
		)
			
		on pck_pickObjects changed state do 
		(
			if state do
			(
				local obj = pickObject filter:filterSources
				if obj != undefined do
				(
					if classof obj == PF_Source then
					(
						--local PossibleParticleGroups = for o in refs.dependents obj where classof o == ParticleGroup AND isProperty o #name collect o
						--for aGroup in PossibleParticleGroups where (findItem VelocitySources aGroup == 0 or not useGlobalGrid ) do
						global Magma_SelectedParticleGroup = #()
						::StokeFieldMFEditor_Functions.getEventFromPFSource obj multiple:true
						if ::Magma_SelectedParticleGroup  != undefined do
						(
							for aGroup in ::Magma_SelectedParticleGroup where findItem VelocitySources aGroup == 0 or not useGlobalGrid do
							(
								append VelocitySources aGroup
								VelocitySourcesOnOff[VelocitySources.count] = true
								append velocityScales 1.0		
								append gridSizes gridSize
								append gridPaddings gridPadding
								append fluidMotions fluidMotion
							)
						)
					)
					else
					(
						append VelocitySources obj
						VelocitySourcesOnOff[VelocitySources.count] = true
						append velocityScales 1.0
						append gridSizes gridSize
						append gridPaddings gridPadding
						append fluidMotions fluidMotion
					)
					updateVelocityList()
				)
			)
			pck_pickObjects.state = false
		)
		
		on btn_addByName click arg do
		(
			theNodes = selectByName filter:filterSources multiple:true showHidden:true
			if theNodes != undefined do
			(
				for obj in theNodes do 
				(
					if classof obj == PF_Source then
					(
						global Magma_SelectedParticleGroup = #()
						::StokeFieldMFEditor_Functions.getEventFromPFSource obj multiple:true
						if ::Magma_SelectedParticleGroup  != undefined do
						(
							for aGroup in ::Magma_SelectedParticleGroup where findItem VelocitySources aGroup == 0 or not useGlobalGrid do
							(
								append VelocitySources aGroup
								VelocitySourcesOnOff[VelocitySources.count] = true
								append velocityScales 1.0
								append gridSizes gridSize
								append gridPaddings gridPadding
								append fluidMotions fluidMotion							
							)
						)
					)
					else
					(
						append VelocitySources obj
						VelocitySourcesOnOff[VelocitySources.count] = true
						append velocityScales 1.0
						append gridSizes gridSize
						append gridPaddings gridPadding
						append fluidMotions fluidMotion						
					)
				)
				updateVelocityList()
			)
		)
		
		on btn_removeSelectedSources click arg do
		(
			local theSel = getListViewSelection dnc_VelocitySources
			if theSel.count > 0 do
			(
				local theControllers = for i = 1 to velocityScales.count collect
				(
					try( this[("velocityScales_"+ (i-1) as string)].controller ) catch (velocityScales[i])
				)
				local tempArray = for o in VelocitySources collect o
				local tempArray2 = for i = 1 to tempArray.count collect VelocitySourcesOnOff[i] != false
				local tempArray3 = for o in velocityScales collect o
					
				local tempArray4 = for o in gridSizes collect o
				local tempArray5 = for o in gridPaddings collect o
				local tempArray6 = for o in fluidMotions collect o
				
				for i = theSel.count to 1 by -1 do
				(
					/*local theItemToDelete = tempArray[theSel[i]]
					local theIndex = findItem tempArray2 theItemToDelete
					if theIndex > 0 do 
					(
						deleteItem tempArray2 theIndex
						VelocitySourcesOnOff = tempArray2
					)*/
					deleteItem tempArray theSel[i]
					deleteItem tempArray2 theSel[i]
					deleteItem tempArray3 theSel[i]
					deleteItem tempArray4 theSel[i]
					deleteItem tempArray5 theSel[i]
					deleteItem tempArray6 theSel[i]
					deleteItem theControllers theSel[i]
				)
				 for i = 1 to velocityScales.count do
				 (
					 if classof theControllers[i] == Bezier_Float then
						try( this[("velocityScales_"+ (i-1) as string)].controller = theControllers[i]) catch ()
					 else
						try(velocityScales[i] = theControllers[i])catch()
				 )
				VelocitySources = for o in tempArray collect o
				VelocitySourcesOnOff = for o in tempArray2 collect o
				velocityScales = for o in tempArray3 collect o
				gridSizes = for o in tempArray4 collect o
				gridPaddings = for o in tempArray5 collect o
				fluidMotions = for o in tempArray6 collect o

				updateVelocityList()
				paramsRollout.updateUI()
				
				if theSel[1] > dnc_VelocitySources.Items.count do theSel[1] =theSel[1]-1
				setListViewSelection dnc_VelocitySources theSel
			)
		)			
		
		on dnc_VelocitySources mouseClick arg do
		(
			local theSel = getListViewSelection dnc_VelocitySources
			if theSel.count > 0 do 
			(
				velocityScale = velocityScales[theSel[1]]
				if arg.Button == arg.Button.Right do
				(
					RCMenuSourceObject = VelocitySources[theSel[1]]
					local theMenu = defineRCMenu()
					popupMenu theMenu pos:mouse.screenpos
				)
			)
		)
		
		on dnc_VelocitySources ItemChecked arg do
		(
			local newArray = #()
			for i = 0 to dnc_VelocitySources.items.count-1 do
			(
				try(VelocitySourcesOnOff[i+1] = dnc_VelocitySources.items.item[i].checked)catch()
			)
			paramsRollout.updateUI()
		)		
		
		on dnc_VelocitySources mouseUp arg do
		(
			updateEnabledStates()
		)
				
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)			
		
		on btn_VelocityScale_Preset pressed do popupPresetMenu #VelocityScale
		on btn_VelocityScale_Preset rightclick do popupPresetMenu #VelocityScale
			
		on btn_gridSize_Preset pressed do popupPresetMenu #gridSize
		on btn_gridSize_Preset rightclick do popupPresetMenu #gridSize
			
		on btn_gridPadding_Preset pressed do popupPresetMenu #gridPadding
		on btn_gridPadding_Preset rightclick do popupPresetMenu #gridPadding		
			
		on chk_useGlobalGrid changed state do 
		(
			updateVelocityList()
			updateEnabledStates()
		)
			
		fn updateUI =
		(
			local theSel = getListViewSelection dnc_VelocitySources
			local isInCreateMode = getCommandPanelTaskMode() == #create
			pck_pickObjects.enabled = btn_addByName.enabled = btn_removeSelectedSources.enabled = not isInCreateMode			
			updateEnabledStates()
			initDistListView()
			updateVelocityList()		
			setListViewSelection dnc_VelocitySources theSel
		)
		
		fn updateObjectsListAnimation =
		(		
			refreshScaleValues()
		)
		
		fn	updateLayout =
		(
			
			btn_addByName.FlatStyle = btn_removeSelectedSources.FlatStyle = btn_addByName.FlatStyle.System
			btn_addByName.width = btn_removeSelectedSources.width = (paramsVelocityFieldRollout.width-42)/2
			btn_addByName.pos.x = 40		
			btn_removeSelectedSources.pos.x = btn_addByName.pos.x + btn_addByName.width + 1
			
			dnc_VelocitySources.width = paramsVelocityFieldRollout.width-4
			dnc_VelocitySources.pos.x = 2
		
			spn_VelocityScale.pos.x = paramsVelocityFieldRollout.width-34
			spn_gridSize.pos.x = spn_gridPadding.pos.x = paramsVelocityFieldRollout.width-38
		)
		
		on paramsVelocityFieldRollout open do 
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
		)
	)	
	
	rollout paramsTimingRollout "Retiming"
	(
		checkbox chk_UsePlaybackTime "Use Graph" across:3 offset:[-8,-3]
		spinner spn_PlaybackTime "" range:[-1000000,1000000,0] type:#float fieldwidth:50 offset:[38,-2] controller:this.delegate.Playbacktime.controller
		button btn_PlaybackTime_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the value of the Playback Graph spinner."
		
		checkbox chk_UsePlaybackInterpolation "Use Playback Interpolation" offset:[-8,-3]
		
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
			rcmenu Stoke_PlaybackTime_Presets_RCMenu 
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
			popUpMenu Stoke_PlaybackTime_Presets_RCMenu position:mouse.screenPos	
		)
			
		on btn_PlaybackTime_Preset pressed do openPlaybackMenu()
		on btn_PlaybackTime_Preset rightclick do openPlaybackMenu()
		
		on chk_UsePlaybackInterpolation changed val do this.delegate.UsePlaybackInterpolation = val
			
		fn updateLayout = 
		(
			spn_PlaybackTime.pos.x = paramsTimingRollout.width-34
		)
			
		on paramsTimingRollout open do
		(
			enableAutoLayout()
			updateLayout()
			chk_UsePlaybackTime.state = this.delegate.UsePlaybackTime
			chk_UsePlaybackInterpolation.state = this.delegate.UsePlaybackInterpolation
		)
	)	
	

	rollout paramsDisplayRollout "Viewport Display"
	(
		checkbox chk_displayInViewport "On" across:3 offset:[-10,-4]
		spinner spn_viewPercentage "Percent " range:[0,100,100] fieldwidth:45 offset:[38,-3] tooltip:"Display a fraction of the simulated particles in the viewport."
		button btn_viewPercentage_Preset ">" width:18 height:16 align:#right offset:[11,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Display a fraction of the simulated particles in the viewport."
		
		checkbox chk_viewLimitEnabled "Limit x1000" across:3 align:#left offset:[-10,-4] tooltip:"When checked, the simulated particles will be displayed in the viewport."
		spinner spn_viewLimit "" range:[1,10000,1000] fieldwidth:45 type:#integer offset:[38,-3] tooltip:"Limit the display to this value multoplied by 1000 to avoid very slow viewport display."
		button btn_viewLimit_Preset ">" width:18 height:16 align:#right offset:[11,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Limit the display to this value multoplied by 1000 to avoid very slow viewport display."

		label lbl_colorSource "Color:" across:2 align:#left offset:[2,3] tooltip:"Set the Color Source"
		dropdownlist ddl_colorSource items:#("Object Color","Color","Velocity","Normal","Tangent") align:#right width:114 offset:[11,-1]

		label lbl_displayVectorSource "Display:" across:2 align:#left offset:[-7,0]
		dropdownlist ddl_displayVectorSource items:#("Large Dots","Velocity") align:#right width:114 offset:[11,-4]
		
		checkbox chk_ViewVectorNormalize "Norm.Length" offset:[-10,-2] across:3 tooltip:"Normalize the Vector channel before displaying it."
		spinner spn_viewVectorScale "" range:[0,1000,1.0] fieldwidth:42 offset:[38,-2] tooltip:"Scale the Vector channel before displaying it."
		button btn_viewVectorScale_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Scale the Vector channel before displaying it."
		
		
		spinner spn_iconSize "Icon Size " range:[0.0,10000.0,30.0] fieldwidth:42 across:2 offset:[60,2] tooltip:"Set the size of the STOKE Viewport Icon."
		button btn_iconSize_Preset ">" width:18 height:16 align:#right offset:[10,2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the STOKE Viewport Icon."
		
		spinner spn_forceUIUpdates "" range:[-100000,1000000,0.0] fieldwidth:42 across:2 offset:[0,-22] visible:false --this is a 
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do 
				popUpMenu Stoke_Presets_RCMenu position:mouse.screenPos		
		)			
		
		on btn_viewPercentage_Preset pressed do popupPresetMenu #viewPercentage
		on btn_viewPercentage_Preset rightclick do popupPresetMenu #viewPercentage
			
		on btn_viewLimit_Preset pressed do popupPresetMenu #viewLimit
		on btn_viewLimit_Preset rightclick do popupPresetMenu #viewLimit
			
		on btn_iconSize_Preset pressed do popupPresetMenu #iconSize
		on btn_iconSize_Preset rightclick do popupPresetMenu #iconSize
			
		on btn_viewVectorScale_Preset pressed do  popupPresetMenu #viewVectorScale
		on btn_viewVectorScale_Preset rightclick do  popupPresetMenu #viewVectorScale
			
		on chk_displayInViewport changed state do this.delegate.ViewportEnabled = state
		on ddl_displayVectorSource selected itm do
		(
			this.delegate.ViewportVectorChannel = case itm of
			(
				1: ""
				default: ddl_displayVectorSource.selected
			)
			this.delegate.ViewportEnabled = this.delegate.ViewportEnabled
		)
		on ddl_colorSource selected itm do
		(
			this.delegate.ViewportColorChannel = ddl_colorSource.selected 
			this.delegate.ViewportEnabled = this.delegate.ViewportEnabled
		)
		
		fn updateUI =
		(
			local theVectors = for aChannel in availableChannels where matchPattern aChannel[2] pattern:"float*[3]" and findItem channelsToSave aChannel[1] > 0 collect aChannel[1]
			local theScalars = for aChannel in availableChannels where matchPattern aChannel[2] pattern:"float*" and not matchPattern aChannel[2] pattern:"*[*]" and findItem channelsToSave aChannel[1] > 0 collect aChannel[1] 

			local colorSources = #("Color","Velocity")
			for i in theVectors do appendIfUnique colorSources i
			for i in theScalars do appendIfUnique colorSources i
				
			ddl_colorSource.items = colorSources
			local displayVectors = #("Large Dots","Velocity")
			for i in theVectors do appendIfUnique displayVectors i
			ddl_displayVectorSource.items = displayVectors
			
			local theIndex = findItem ddl_displayVectorSource.items this.delegate.ViewportVectorChannel
			if theIndex == 0 do theIndex = 1
			ddl_displayVectorSource.selection = theIndex
			
			local theIndex = findItem ddl_colorSource.items this.delegate.ViewportColorChannel
			if theIndex == 0 do theIndex = 1
			ddl_colorSource.selection = theIndex
			
			chk_displayInViewport.checked = this.delegate.ViewportEnabled
		)
		
		fn updateLayout = 
		(
			spn_viewPercentage.pos.x = spn_viewLimit.pos.x = spn_viewVectorScale.pos.x = spn_iconSize.pos.x = paramsDisplayRollout.width-32
			ddl_colorSource.width = ddl_displayVectorSource.width = paramsDisplayRollout.width-50
			ddl_colorSource.pos.x= ddl_displayVectorSource.pos.x = 50
		)
		
		on paramsDisplayRollout open do
		(
			enableAutoLayout()
			updateLayout()
			updateUI()
		)
	)

	fn onMagmaError msg NodeID:unsupplied ExpressionID:unsupplied = 
	(	
		local reportingHolder = if ExpressionID == 0 then delegate.SimulationMagma else delegate.BirthMagma
		local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == reportingHolder collect i)

		if msg != undefined then
		(
			if ExpressionID != unsupplied do
			(
				if ExpressionID == 0 then 
				(
					--print "Error!"
					paramsRollout.prg_MagmaError.color = red
					LastErrorID = nodeID
					LastErrorMsg = msg
					if existingEditor.count == 1 do
						existingEditor[1][2].updateErrorInfo NodeID:NodeID ErrorMessage:msg 
				)
				else
				(
					paramsRollout.prg_BirthMagmaError.color = red
					LastBirthErrorID = nodeID
					LastBirthErrorMsg = msg
					if existingEditor.count == 1 do
						existingEditor[1][2].updateErrorInfo NodeID:NodeID ErrorMessage:msg
				)
			)
			stopSimDueToMagmaError = true
		)
		else
		(
			if ExpressionID == 0 then
			(
				--print "No Error"
				LastErrorID = -100
				paramsRollout.prg_MagmaError.color = green
				if existingEditor.count == 1 do
					existingEditor[1][2].updateErrorInfo NodeID:-100 ErrorMessage:""
			)
			else
			(
				LastBirthErrorID = -100
				paramsRollout.prg_BirthMagmaError.color = green
				if existingEditor.count == 1 do
					existingEditor[1][2].updateErrorInfo NodeID:-100 ErrorMessage:""
			)
		)
		
	)
	
	on create do
	(
		seed (timestamp())
		randomID = (random 1000 1000000) as string + "_" + (random 1000 1000000) as string
		local theDefaultPath = getIniSetting (getDir #plugcfg + "\\StokePreferences.ini") "Output" "Default"
		if theDefaultPath == "" do theDefaultPath = "$default\\" --(dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\Stoke\\Cache\\"
		outputPath = theDefaultPath
		theDirs = getDirectories (getCachePath() + "Stoke_"+randomID+"\\*")
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
		pathToSave = (getCachePath()+"Stoke_"+randomID+"\\"+outputVersion)
		makedir pathToSave all:true
		this.delegate.InitializeParticleCache (pathToSave+"\\"+outputPrefix+"_####.prt")
		this.delegate.Playbacktime.controller = bezier_float()
		this.channelsToSave = #("Velocity","ID","Age","LifeSpan")
		--this.delegate.SequenceCacheCapacityMB = 4096
		
		local theVal=getIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level"
		if theVal != "" do try(StokeGlobalInterface.LoggingLevel = theVal as name)catch()		

			
		this.delegate.UseMagma = true --enable magma evaluation by default	
		--Create The Simulation Magma Flow
		this.delegate.InitSimMagmaHolder()
			
		--animate the force UI updates parameter
		local floatEx = float_expression()
		forceUIUpdates.controller = floatEx
		floatEx.setExpression "F"

		this.delegate.SetCallback onMagmaError
	)

	on load do
	(
		if this.delegate.Playbacktime.controller == undefined do 
			this.delegate.Playbacktime.controller = bezier_float()
		
		--Make sure old scenes have correct per-object Grid settings:
		for v = 1 to velocitySources.count do
		(
			if gridSizes[v] == undefined do gridSizes[v] = gridSize
			if gridPaddings[v] == undefined do gridPaddings[v] = gridPadding
			if fluidMotions[v] == undefined do fluidMotions[v] = fluidMotion
		)

		if this.delegate.SimulationMagma == undefined or this.delegate.BirthMagma == undefined do
			this.delegate.InitSimMagmaHolder()
		
		--animate the force UI updates parameter
		local floatEx = float_expression()
		forceUIUpdates.controller = floatEx
		floatEx.setExpression "F"
		
		this.delegate.SetCallback onMagmaError
	)
)


macroScript Stoke category:"Stoke" tooltip:"Create Stoke Object. Hold SHIFT to create at World Origin." icon:#("Stoke",1)
(
	local theSel = selection as array
	fn filterPRTObjects StokeObj obj =
	(
		(StokeGlobalInterface.GetSourceType obj ) != #invalid and findItem StokeObj.distSources obj == 0
	)
	
	fn filterSources StokeObj obj = 
	(
		--findItem StokeObj.VelocitySources obj == 0 and (WSMSupportsForce obj or (StokeGlobalInterface.GetSourceType obj ) == #particles or findItem #(StokeFieldSim,Stoke_Field,FumeFX) (classof obj.baseobject) > 0)
		(WSMSupportsForce obj or (StokeGlobalInterface.GetVelocityType obj ) != #invalid)
	)
	
	fn populateFromSelection theSel theObj =
	(
		for o in theSel do
		(
			if filterPRTObjects theObj o do
			(
				local theType = (StokeGlobalInterface.GetSourceType o)
				append theObj.distSources o
				append theObj.distSourcesOnOff True
				append theObj.distRates 100.0
				append theObj.geometryMode ""
				append theObj.distSourcesSelectionType ""
				append theObj.distSeedAsRate false
				append theObj.distUseNewParticlesOnly true
						
				if theType == #geometry then
				(
					append theObj.distJitterRadius 0.0
					local theBBox = o.max-o.min
					local theMaxSize = amax #(theBBox.x, theBBox.y, theBBox.z)
					append theObj.distVolumeSpacing (theMaxSize/50.0)						
				)
				else
				(
					append theObj.distJitterRadius 0.0
					append theObj.distVolumeSpacing 1.0
				)
			)
			if filterSources theObj o do
			(
				if classof o == PF_Source do
				(
					local PossibleParticleGroups = for i in refs.dependents o where classof i == ParticleGroup AND isProperty i #name collect i
					if PossibleParticleGroups.count > 0 do o = PossibleParticleGroups[1] 
				)
				if findItem theObj.VelocitySources o == 0 do
				(
					append theObj.VelocitySources o
					append theObj.VelocitySourcesOnOff true
					append theObj.velocityScales 1.0				
					append theObj.gridSizes theObj.gridSize
					append theObj.gridPaddings theObj.gridPadding
					append theObj.fluidMotions theObj.fluidMotion					
				)
			)
		)
	)
	if keyboard.shiftpressed then
	(
		local theObj = Stoke pos:[0,0,0] iconSize:30.0 wirecolor:(color 255 225 125)
		theObj.startTime = animationrange.start
		theObj.endTime = animationrange.end
		populateFromSelection theSel theObj 
		select theObj
		max modify mode
	)
	else
	(
		tool StokeCreator
		(
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						in coordsys grid theObj=Stoke pos:gridPoint wirecolor:(color 255 225 125)
						theObj.startTime = animationrange.start
						theObj.endTime = animationrange.end
						populateFromSelection theSel theObj 
						select theObj
						max modify mode
						#stop
					)
				)
			)
		)
		startTool StokeCreator
	)
)

callbacks.removeScripts id:#stokeReset
callbacks.addScript #systemPreReset "for o in (GetClassInstances Stoke) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetParticleCache() )" id:#stokeReset
callbacks.addScript #filePreOpen "if callbacks.notificationParam() != 2 do for o in (GetClassInstances Stoke) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetParticleCache() )" id:#stokeReset
callbacks.addScript #systemPreNew "if callbacks.notificationParam() == 1 do (for o in (GetClassInstances Stoke) where (o.isAsyncFlushActive != true) do ( o.delegate.ResetParticleCache() ))" id:#stokeReset
callbacks.addScript #preSystemShutdown "::StokeParticleSimulationShutdownCallback()" id:#stokeReset


macroScript StokeLogLevelNone category:"Stoke" tooltip:"Set Stoke Log Level To NONE." icon:#("Stoke",15)
(
	on isChecked do StokeGlobalInterface.LoggingLevel == #none
	on execute do 
	(
		StokeGlobalInterface.LoggingLevel= #none
		setIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level" (StokeGlobalInterface.LoggingLevel as string)
	)
)
macroScript StokeLogLevelWarning category:"Stoke" tooltip:"Set Stoke Log Level To WARNING." icon:#("Stoke",14)
(
	on isChecked do StokeGlobalInterface.LoggingLevel == #warning
	on execute do 
	(
		StokeGlobalInterface.LoggingLevel= #warning
		setIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level" (StokeGlobalInterface.LoggingLevel as string)
	)
)

macroScript AboutStoke category:"Stoke"
(
	global Stoke_About_Dialog
	try(destroyDialog Stoke_About_Dialog)catch()
	rollout Stoke_About_Dialog "About Stoke MX"
	(
		label lbl_about10 "STOKE MX"
		label lbl_about20 "by AWS Thinkbox" offset:[0,-3]
		
		edittext edt_version "Version:" text:"" readonly:true fieldwidth:150 align:#left across:2
		edittext edt_dlrdate "Plug-in DLL Date:" text:"" readonly:true fieldwidth:150 align:#right
		edittext edt_installationfolder "Installation Folder:" text:"" readonly:true width:515 align:#right offset:[0,-3]
		
		label lbl_attributions "3rd Party Software Attributions:" align:#left offset:[0,2] across:2
		button btn_copyattributions "Copy To Clipboard" align:#right height:20 offset:[0,-3]
		dotNetControl dnc_attributions "System.Windows.Forms.TextBox" width:520  height:400 offset:[0,-4] align:#center
		
		on btn_copyattributions pressed do 
		(
			dnc_attributions.SelectAll()
			dnc_attributions.Copy()
		)

		on Stoke_About_Dialog open do
		(
			edt_version.text = StokeGlobalInterface.Version
			local maxVer = (((maxVersion())[1]/1000)-2) as string
			if maxVer == "16" do maxVer = "15"			
			if maxVer == "14" do maxVer = "13"
			edt_installationfolder.text = StokeGlobalInterface.HomeDirectory
			edt_dlrdate.text = try(getFileModDate (StokeGlobalInterface.HomeDirectory + "3dsMax20"+maxVer+"\\StokeMX.dlo"))catch("???")
			try
			(
				dnc_attributions.multiline = true
				dnc_attributions.AcceptsReturn = false
				dnc_attributions.AcceptsTab = false
				dnc_attributions.WordWrap = false
				dnc_attributions.ScrollBars= dnc_attributions.ScrollBars.Both
				
				local theColor = (colorman.getColor #window) * [255,255,270]
				dnc_attributions.BackColor = dnc_attributions.BackColor.fromARGB theColor.x theColor.y theColor.z

				local theColor = (colorman.getColor #windowtext) * 255
				dnc_attributions.ForeColor = dnc_attributions.ForeColor.fromARGB theColor.x theColor.y theColor.z
				
				local txt = StokeGlobalInterface.Attributions
				txt = substituteString txt  "\n" "\r\n"
				dnc_attributions.text = txt
			)
			catch
			(
				dnc_attributions.text = "No attributions text found in the Stoke Plug-in DLL."
			)
		)
	)
	createDialog Stoke_About_Dialog 540 510
)