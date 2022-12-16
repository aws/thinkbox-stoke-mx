-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
MacroScript StokePartitioning category:"Stoke" tooltip:"Stoke Partitioning - Create multiple variations of the same simulation to increase density." icon:#("Stoke",2)
(
	global STOKE_PartitioningDialog
	try(destroyDialog STOKE_PartitioningDialog)catch()
	
	global Stoke_Progress_Cancel = false
	global Stoke_PresetsArrowBitmap
	if Stoke_PresetsArrowBitmap == undefined do Stoke_PresetsArrowBitmap = bitmap 640 32
	
	local submitToDeadlineControls = false
	local collectedPools = false
	local deadlineVersion = #default
	local deadlineFound = true
	
	struct StokePartCallback
	(
		stokePlugin = undefined,
		stokeSim = undefined,
		
		particleSources = undefined,
		velocityFields = undefined,
		
		storageCounter = 0,
		updateCounter = 0,
		--theMaxVersion = (maxVersion())[1],
		
		fn onFrameUpdateUI theTime = 
		(
			local progressValue = 0.0
			if theTime > stokePlugin.startTime do 
				progressValue = (100.0 * (theTime-stokePlugin.startTime) / (stokePlugin.endTime-stokePlugin.startTime) ) as integer
			
			--if theMaxVersion > 12000 do 
			windows.processPostedMessages()
			
			STOKE_PartitioningDialog.prg_progress.value = progressValue
			STOKE_PartitioningDialog.btn_cancel.text = "PRESS TO STOP - CURRENT PARTITION: " + progressValue as string + "%"
			--if theMaxVersion < 13000 do ::Stoke_Progress_Cancel = not (progressUpdate progressValue)
			not ::Stoke_Progress_Cancel
		),
		
		fn onFrameUpdate = 
		(
			local theTime = stokeSim.GetCurrentTime()
			
			if not stokePlugin.useCacheStartTime or theTime >= stokePlugin.CacheStartTime do
				stokePlugin.delegate.AddParticleSet ( stokeSim.GetCurrentParticles() ) theTime --this caches to memory and allows PRT saving 
			
			if ( onFrameUpdateUI theTime ) then 
			(
				at time theTime 
				( 
					stokePlugin.updateParticleSourceRates particleSources
					stokePlugin.updateVelocitySourceScales velocityFields
				)
				true
			) else false
		)
	)	
	
	rollout STOKE_PartitioningDialog "STOKE Partitioning"
	(
		local theStokeObjects = #()
		local StokeIniPath = GetDir #plugcfg + "//StokePreferences.ini"
		
		label lbl_stokeObjects "Select a STOKE object to Partition:" across:2 align:#left offset:[-5,-1]
		button btn_updateStokeList "Update List" align:#right offset:[7,-3] height:20 
		dropdownlist ddl_stokeObjects "" width:225 align:#left  offset:[-7,-5] across:2 
		edittext edt_driveinfo offset:[7,-3] fieldwidth:160 align:#right readonly:true		
		
		spinner spn_partitionCount "Partition Count: " range:[2,100,10] fieldwidth:30 type:#integer align:#left offset:[0,0] across:3
		spinner spn_PartitionStart "Partition Range From: " range:[1,10,1] type:#integer align:#center offset:[40,0] fieldwidth:30 
		spinner spn_PartitionEnd "To: " range:[1,10,10] type:#integer align:#right offset:[0,0] fieldwidth:30 

		group "Options"
		(
			checkbox chk_useMemoryLimit "Override Memory Limit"  across:2 align:#left
			spinner spn_cacheMemory "Memory Limit in MB:"  type:#integer range:[0,65536,4096] fieldwidth:40 align:#right
			checkbox chk_overrideThreads "Override Max. Saving Threads" align:#left offset:[0,0] across:2
			spinner spn_MaxThreads "Max. Saving Threads:" range:[1,sysinfo.cpucount,1] type:#integer fieldwidth:40 align:#right offset:[0,0]
			checkbox chk_createPRTLoader "Create PRT Loader After Saving" checked:true align:#left offset:[0,0]
		)
		progressbar prg_partitionsProgress height:8 color:red width:390 align:#center offset:[0,0]
		progressbar prg_progress height:8 color:green width:390 align:#center offset:[0,-3]
		button btn_partition "PARTITION CUSTOM RANGE ON WORKSTATION" width:390 height:30 align:#center offset:[0,-3] enabled:false
		button btn_cancel "CANCEL" width:390 height:30 align:#center offset:[0,-35] visible:false
		
		button btn_submitToDeadlineControls "EXPAND Deadline Submission Controls..." width:390 align:#center height:18

		group "Save To Network Base Path:"
		(
			button btn_getNetworkPath "..." width:16 height:17 align:#left offset:[-6,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,11,11,11,11) across:3
			edittext edt_networkPath fieldwidth:350 align:#center offset:[-2,-3] 
			button btn_exploreNetworkPath "..." width:16 height:17 align:#right offset:[7,-3] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,12,12,12,12) 
		)
		
		group "Submit To Deadline"
		(
			edittext edt_comment "Comment:" fieldwidth:320 align:#left offset:[1,0] 
			
			edittext edt_user "Name:" fieldwidth:140 align:#left offset:[19,-3] across:2
			edittext edt_dept "Dept:" fieldwidth:140 align:#right offset:[0,-3]

			label lbl_pools "Pool:" align:#left offset:[26,0] across:4
			dropdownList ddl_poollist "" width:141 align:#right offset:[9,-3]
			
			label lbl_category "Group:" align:#left offset:[12,0] 
			dropdownlist ddl_groupList items:#() align:#right width:141 offset:[3,-3]
			
			label lbl_priority "Priority:" align:#left offset:[12,0] across:3
			progressbar sld_priority width:270 height:18 range:[0,100,50] type:#integer align:#center offset:[3,-2] value:50
			spinner spn_priority "" type:#integer fieldwidth:35 align:#right  offset:[0,-1] range:[1,100,50]
			
			spinner spn_machineLimit "Machine Limit:" range:[0,1000,0] type:#integer fieldwidth:35 offset:[0,0] align:#right tooltip:"This value defines the number of Deadline Workers that are allowed to process Tasks of the Partitioning Job at the same time.\n\nIt is a good idea to limit the number of Workers when saving very large PRT files over the network."

			checkbox chk_useThreadLimitOnDeadline "Limit Number of Saving Threads" across:2 align:#left offset:[0,-2]
			spinner spn_ThreadLimitOnDeadline "Max. Saving Threads:" range:[1,16,1] type:#integer fieldwidth:35 align:#right
			
		)
		
		button btn_SubmitToDeadline "PARTITION CUSTOM RANGE ON DEADLINE" width:392 height:30 align:#center offset:[0,-3] enabled:false


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
	
		fn getFreeSpaceOnDisk =
		(
			if theStokeObjects.count == 0 do return false
			local ss = "" as stringStream
			local theStokeObject = theStokeObjects[ddl_stokeObjects.selection]
			if isValidNode theStokeObject do
			(
				local thePath = theStokeObject.getCachePath()
				local dnc = dotNetClass "System.IO.DriveInfo"
				local theDrives = dnc.GetDrives()
				for aDrive in theDrives do
				(
					if matchPattern thePath pattern:(aDrive.RootDirectory.Name+"*") do
						format "% % GB, %, %  " aDrive.Name (addCommas ((aDrive.AvailableFreeSpace/1024.0/1024/1024) as string)) aDrive.DriveFormat (aDrive.DriveType.ToString())   to:ss
				)
			)
			if ss as string == "" do ss = "Disk Space Data Not Available"
			edt_driveinfo.text = ss as string
		)		

		fn job_priority_update val =
		(
			if val <= 100 do
			(
				theRed = (255.0 - 255.0*val/100.0)*2.0
				if theRed > 255 do theRed = 255
				theGreen = 512.0*val/100.0
				if theGreen > 255 do theGreen = 255
				sld_priority.color = [theRed, theGreen, 0]
				sld_priority.value = spn_priority.value = val
			)
			val
		)	
		
		fn updateButtonEnabledStates =
		(
			if ddl_stokeObjects.selection > 0 then
			(
				btn_partition.enabled = true
				btn_partition.text = "PARTITION CUSTOM RANGE ON WORKSTATION"
				if deadlineFound then
				(	
					if doesFileExist edt_networkPath.text and pathIsNetworkPath edt_networkPath.text then 
					(
						btn_SubmitToDeadline.enabled = true
						btn_SubmitToDeadline.text = "PARTITION CUSTOM RANGE ON DEADLINE"
					)
					else
					(
						btn_SubmitToDeadline.enabled = false
						btn_SubmitToDeadline.text = "PLEASE SPECIFY A VALID NETWORK PATH"					
					)
				)
				else
				(
					btn_SubmitToDeadline.enabled = false
					btn_SubmitToDeadline.text = "FAILED TO DETECT DEADLINE ON THIS SYSTEM"					
				)
			)
			else
			(
					btn_partition.enabled = btn_SubmitToDeadline.enabled = false
					btn_SubmitToDeadline.text = "NEED A STOKE OBJECT TO PARTITION"	
					btn_partition.text = "NEED A STOKE OBJECT TO PARTITION"	
			)
			getFreeSpaceOnDisk()
		)
		
		
		on spn_priority changed value do setIniSetting StokeIniPath "JobSettings"  "Priority" ( (job_priority_update value) as string )
		on sld_priority clicked value do setIniSetting StokeIniPath "JobSettings"  "Priority" (  (job_priority_update value) as string )
		on spn_machineLimit changed val do setIniSetting StokeIniPath "JobSettings"  "MachineLimit" (val as string)
		
		on edt_user entered txt do setIniSetting StokeIniPath "JobSettings"  "UserName" txt
		on edt_dept entered txt do setIniSetting StokeIniPath "JobSettings"  "Department" txt
		on ddl_poollist selected itm do setIniSetting StokeIniPath "JobSettings"  "Pool" ddl_poollist.selected
		on ddl_grouplist selected itm do setIniSetting StokeIniPath "JobSettings"  "Group" ddl_grouplist.selected
			
		on btn_getNetworkPath pressed do 
		(
			local thePath = getSavePath initialDir:edt_networkPath.text
			if thePath != undefined do 
			if pathIsNetworkPath thePath then
				setIniSetting StokeIniPath "JobSettings" "NetworkPath" (edt_networkPath.text = thePath)
			else
				messagebox "The Selected Path Is Not A Network Path!" title:"Stoke Partitioning On Deadline"
			updateButtonEnabledStates()
		)
		
		on edt_networkPath changed txt do
		(
			if doesFileExist txt and pathIsNetworkPath txt do 
					setIniSetting StokeIniPath "JobSettings" "NetworkPath" txt
			updateButtonEnabledStates()
		)		
		
		on btn_exploreNetworkPath pressed do shellLaunch "explorer.exe" edt_networkPath.text
		
		fn LeadingZeros value count =
		(
			local theStr = value as string
			local theCount = count-(theStr.count)
			if theCount < 1 then "" else substring "00000000000" 1 theCount
		)	
		
		on spn_partitionCount changed val do 
		(
			if spn_PartitionStart.value > val do spn_PartitionStart.value = val
			if spn_PartitionEnd.value > val do spn_PartitionEnd.value = val
			spn_PartitionStart.range = [1,val,spn_PartitionStart.value]
			spn_PartitionEnd.range = [1,val,spn_PartitionEnd.value]
		)
		on spn_PartitionEnd changed val do 
		(
			if val < spn_PartitionStart.value do spn_PartitionStart.value = val
		)
		on spn_PartitionStart changed val do 
		(
			if val > spn_PartitionEnd.value do spn_PartitionEnd.value = val
		)		
		
		fn partitionStokeObject theStokeObject =
		(
			::Stoke_Progress_Cancel = false
			escapeEnable = false
			--local theMaxVersion = (maxVersion())[1]
			local thePartCount = spn_partitionCount.value
			local theMemoryLimit = if chk_useMemoryLimit.checked then spn_cacheMemory.value else theStokeObject.memoryLimit
			local theFixedBufferSize = 512
			if theMemoryLimit < 1024 do
			(
				theMemoryLimit = 1024
				theFixedBufferSize = 128
			)
			theStokeObject.delegate.SequenceCacheCapacityMB = theFixedBufferSize
			theStokeObject.delegate.SerializeQueueCapacityMB = theMemoryLimit - theFixedBufferSize
			local theThreadsCount = if chk_overrideThreads.checked then 
				spn_MaxThreads.value
			else
				sysinfo.cpucount
			
			theStokeObject.delegate.SetNumSerializerThreads  theThreadsCount

			if theStokeObject.delegate.SimulationMagma == undefined or theStokeObject.delegate.BirthMagma == undefined do
			(
				theStokeObject.delegate.InitSimMagmaHolder()
			)
				
			local oldSaveState = theStokeObject.saveResultsToDisk 
			theStokeObject.saveResultsToDisk = true
			
			local thePathsArray = #()
			local theFlagsArray = #()

			local st0 = timestamp()
			--max create mode
			local oldSeed = theStokeObject.randomSeed
			btn_cancel.visible = true
			btn_partition.visible = false
			
			local fromPartition = spn_PartitionStart.value
			local toPartition = spn_PartitionEnd.value
			
			
			format "--STOKE PARTITIONING RANGE FROM % TO %\n" fromPartition toPartition
			format "--Saving using % of % threads.\n" theThreadsCount sysinfo.cpucount
			
			local cnt = 0
			for p = fromPartition to toPartition while not ::Stoke_Progress_Cancel do
			(
				cnt += 1
				prg_partitionsProgress.value = 100.0*cnt/(toPartition-fromPartition+1)
				theStokeObject.randomSeed += (p-1)
				local theCountString = thePartCount as string
				local outputPrefix = "STKPRT_"+theStokeObject.outputPrefix+"_part"+ (LeadingZeros p theCountString.count)+ p as string + "of"+theCountString+"_"
				local outPath = theStokeObject.getCachePath() + "\\Stoke_" +theStokeObject.randomID+"\\" + theStokeObject.outputVersion + "\\"
				theStokeObject.delegate.InitializeParticleCache ( outPath+outputPrefix+"_####.prt" )	
				append thePathsArray (outPath+outputPrefix+"_0000.prt")
				append theFlagsArray (if p == 1 then 3 else 2)
				
				format "\t--STOKE Partition % of % to: [%]\n" p thePartCount (outPath+outputPrefix+"_####.prt")

				local theParticleObjects = for i = 1 to theStokeObject.distSources.count where theStokeObject.distSourcesOnOff[i] != false collect theStokeObject.distSources[i]
				local theParticleSources = theStokeObject.collectParticleSources()
				local theVelocityFields = theStokeObject.collectVelocitySources()
				local theExtraChannels = theStokeObject.collectParticleChannels  theParticleSources
				
				
				local theChannelsToSaveMap = join #("Position float32[3]") (deepCopy theExtraChannels)
				local internalChannels = #(#("Velocity","float16[3]"),#("ID","int64[1]"), #("Age","float16[1]"),#("LifeSpan","float16[1]"),#("NormalizedAge","float16[1]"))
				for i in internalChannels where findItem theStokeObject.channelsToSave i[1] > 0 do
				(		
					local theChannel = i[1]
					local theFormat = i[2]
					for j in theStokeObject.channelsToSaveFormat where matchPattern j pattern:(theChannel+"*") do 
					(
						if matchPattern j pattern:"*32*" do theFormat = substituteString theFormat "16" "32"
					)
					appendIfUnique theChannelsToSaveMap (theChannel+" "+theFormat)
				)			
				theStokeObject.delegate.SetChannelsToSave theChannelsToSaveMap --set the channels to be saved
				format "\t\t--Save Channel Map:\n" 
				for i in theChannelsToSaveMap do
					format "\t\t\t--%\n" i
							
				
				local theSim = StokeSimulator()
				
				theSim.SetSimulationRange theStokeObject.startTime theStokeObject.endTime numSubSteps:theStokeObject.subSteps
				theSim.SetParticleChannels theExtraChannels
				theSim.EnableParticleDeath theStokeObject.useLifeSpan

				theSim.SetBirthMagmaHolder theStokeObject.delegate.BirthMagma
				theSim.SetSimMagmaHolder theStokeObject.delegate.SimulationMagma
				theSim.SetErrorReporter theStokeObject.delegate.MagmaErrorReporter
				theSim.setUseMagma theStokeObject.delegate.UseMagma
				theStokeObject.LastErrorID = -100
				theStokeObject.LastBirthErrorID = -100
				theSim.SetStokeObject theStokeObject
				
				for pg in theParticleSources where pg != undefined do 
					theSim.AddParticleSource pg
				
				local validFields = 
					for vf in theVelocityFields where vf != undefined collect vf
				
				local theVelocityField = if validFields.count > 1 then 
					( StokeGlobalInterface.CreateAdditiveVelocityField validFields ) else
					( validFields[1] )
					
				theSim.SetVelocitySource theVelocityField
				
				theStokeObject.delegate.ResetParticleCache()
				
				--if theMaxVersion < 13000 do progressStart "Simulating..."
				
				if not theStokeObject.useViewportParticles do 
					StokeGlobalInterface.BeginRenderMode theParticleObjects
				
				local st = timestamp()
				local theCallback = StokePartCallback stokePlugin:theStokeObject stokeSim:theSim particleSources:theParticleSources velocityFields:theVelocityFields
				local result = theSim.Simulate frameCallback:(theCallback.onFrameUpdate)
				format "\t\t--Generate Time: % sec.\n" (theSim.totalGenerateTime /1000.0)
				format "\t\t--Advect Time: % sec.\n" (theSim.totalAdvectTime /1000.0)
				format "\t\t--Update Time: % sec.\n" (theSim.totalUpdateTime /1000.0)
				format "\t\t--STOKE Partition % of % SIMULATION Time: % sec.\n" p thePartCount ((timestamp()-st)/1000.0)

				-- NB! Must call EndRenderMode with same list as BeginRenderMode
				if not theStokeObject.useViewportParticles do 
					StokeGlobalInterface.EndRenderMode theParticleObjects

				--if theMaxVersion < 13000 do progressEnd()

				local st2 = timestamp()
				btn_cancel.text = "FLUSHING CACHE TO DISK, PLEASE WAIT..."
				--if theMaxVersion > 12000 do 
					windows.processPostedMessages()
				theStokeObject.delegate.FlushParticleCache()
				theStokeObject.randomSeed = oldSeed
				format "\t\t--STOKE Partition % of % CACHE FLUSH Time: % sec.\n" p thePartCount ((timestamp()-st2)/1000.0)
				format "\t--STOKE Partition % of % TOTAL Time: % sec\n" p thePartCount ((timestamp()-st)/1000.0) 
			)--end p loop
			theStokeObject.saveResultsToDisk = oldSaveState
			theStokeObject.delegate.InitializeParticleCache ""
			
			format "--TOTAL STOKE PARTITIONING TIME: % sec.\n\n" ((timestamp()-st0)/1000.0)
			escapeEnable = true
			prg_progress.value = 0
			prg_partitionsProgress.value  = 0
			btn_cancel.visible = false
			btn_partition.visible = true
			
			if chk_createPRTLoader.checked do
			(
				local loaderObject = KrakatoaPRTLoader name:(uniquename ("PRTL_"+theStokeObject.name + "_"))
				loaderObject.fileList = thePathsArray
				loaderObject.fileListFlags = theFlagsArray
				loaderObject.percentViewport = 100.0
				loaderObject.useViewportLimit = true
				loaderObject.viewportLimit = 1000
				loaderObject.viewportParticleDisplayMode = 3
				loaderObject.wirecolor = color 255 200 100	
				select loaderObject				
			)
			
		)--end partition fn 
		
		local jobName = ""
		local theFrameSequence = ""
		local SubmitInfoFileName = GetDir #temp + "\\stokemx_submit_info.job"
		local JobInfoFileName = GetDir #temp + "\\stokemx_job_info.job"
		
		fn getMaxVersion =
		(
			if( ((maxVersion())[1]/1000 as integer) > 9 ) then
				((((maxVersion())[1]/1000 as integer) + 1998) as string)
			else
				(((maxVersion())[1]/1000 as integer) as string)
		)		
		
		fn CreateSubmitInfoFile filename =
		(
			local submitInfoFile = CreateFile filename
			if (submitInfoFile != undefined) then
			(
				format "Plugin=3dsmax\n" to:submitInfoFile
				format "Frames=%\n" 				theFrameSequence 				to:submitInfoFile
				format "ChunkSize=1\n" 													to:submitInfoFile
				format "Priority=%\n" 				spn_Priority.value	 	 			to:submitInfoFile
				format "Pool=%\n" 					ddl_poollist.selected				to:submitInfoFile
				
				format "Name=%\n" 				jobName 	to:submitInfoFile
				format "UserName=%\n"		edt_user.text								to:submitInfoFile
				format "Comment=%\n" 		edt_comment.text						to:submitInfoFile
				format "Department=%\n" 		edt_dept.text								to:submitInfoFile				
				format "Group=%\n" 				ddl_groupList.selected 					to:submitInfoFile				
				
				local theStokeObject = theStokeObjects[ddl_stokeObjects.selection]
				local outputFilenameIndex = 0
				for p = spn_partitionStart.value to spn_partitionEnd.value do
				(
					local theCountString = spn_partitionCount.value as string
					local outputPrefix = "STKPRT_"+theStokeObject.outputPrefix+"_part"+ (LeadingZeros p theCountString.count)+ p as string + "of"+theCountString+"_"
					local outPath = edt_networkPath.text + "\\Stoke_" +theStokeObject.randomID+"\\" + theStokeObject.outputVersion + "\\"
					local thePartPath = ( outPath+outputPrefix+"_####.prt" )	
					format "OutputDirectory%=%\n" outputFilenameIndex	(getFilenamePath thePartPath) 	to:submitInfoFile
					format "OutputFilename%=%\n" outputFilenameIndex (filenameFromPath thePartPath) to:submitInfoFile
					outputFilenameIndex +=1
				)
				
				format "MachineLimit=%\n" spn_machineLimit.value to:submitInfoFile
				close submitInfoFile
				true
			)
			else
				false
		)
	
		--THIS IS NOW CALLED PLUGIN INFO FILE!
		fn CreateJobInfoFile filename =
		(
			local JobInfoFile = CreateFile filename
			if (JobInfoFile != undefined) then
			(
				local maxVersionToUse = getMaxVersion()
				format "Version=%\n" maxVersionToUse to:JobInfoFile
				
				if( ((maxVersion())[1]/1000 as integer) >= 12 and ((maxVersion())[1]/1000 as integer) < 18 ) then
				(
					local maxProductID = try(maxOps.productID)catch(#3dsmax)
					if( maxProductID == #3dsmax ) then
						format "IsMaxDesign=0\n" to:JobInfoFile
					else
						format "IsMaxDesign=1\n" to:JobInfoFile
				)
				else
					format "IsMaxDesign=0\n" to:JobInfoFile
				
				format "IgnoreMissingExternalFiles=0\n" to:JobInfoFile
				format "IgnoreMissingUVWs=0\n"  to:JobInfoFile
				format "IgnoreMissingDLLs=0\n"  to:JobInfoFile
				format "IgnoreMissingXREFs=0\n"  to:JobInfoFile
				
				format "DisableMultipass=1\n" to:JobInfoFile 
				format "OneCpuPerTask=0\n" to:JobInfoFile
				
				format "UseSilentMode=1\n" to:JobInfoFile
				format "UseSilentMode=0\n" to:JobInfoFile
				
				format "RestartRendererMode=0\n"  to:JobInfoFile

				format "OverrideFailOnExistingMaxProcess=true\n" to:JobInfoFile
				format "FailOnExistingMaxProcess=false\n" to:JobInfoFile
				
				format "MaxVersionToForce=none\n" to:JobInfoFile
				format "IgnoreRenderElements=1\n" to:JobInfoFile
				format "RenderOutput=\n" to:JobInfoFile				
				format "MAXScriptJob=1\n" to:JobInfoFile
				
				format "NetworkPath=%\n" edt_networkPath.text to:JobInfoFile
				format "TotalPartitions=%\n" spn_partitionCount.value to:JobInfoFile
				format "StokeObject=%\n" ddl_stokeObjects.selected to:JobInfoFile

				format "UseThreadLimit=%\n" chk_useThreadLimitOnDeadline.state to:JobInfoFile
				format "ThreadLimit=%\n" spn_ThreadLimitOnDeadline.value to:JobInfoFile

				Close JobInfoFile
				true
			)
			else
				false
		)
		
		local submitOutputFile = sysInfo.tempdir + "submitOutput.txt"
		local submitExitCodeFile = sysInfo.tempdir + "submitExitCode.txt"
		
		fn waitForCommandToComplete params timeOutInSec  =
		(
			local result = -2
			deleteFile submitOutputFile
			deleteFile submitExitCodeFile
			
			local binDir = case deadlineVersion of
			(
				#deadline6: (systemTools.getEnvVariable( "DEADLINE_PATH" ) + "\\")
				default: ("")
			)
			local DeadlineBGExec = BinDir + "deadlinecommandbg.exe" 
			
			ShellLaunch DeadlineBGExec ("-outputfiles \"" + submitOutputFile + "\" \"" + submitExitCodeFile + "\" " + params)
			
			local startTimeStamp = timestamp()
			local ready = false
			while not ready do
			(
				sleep 0.15
				if doesFileExist submitExitCodeFile do
				(
					local theFile = openFile submitExitCodeFile
					try(result = readValue theFile)catch(result = -2)
					try(close theFile)catch()
					ready = true
				)	
				if timestamp() - startTimeStamp > timeOutInSec*1000 then 
				(
					result = -3
					ready = true
				)	
			)				
			return case result of
			(
				0: #success
				(-1): #failed
				(-2): #readerror
				(-3): #timeout
			)	
		)		
		
		fn ReadFileIntoArray theFilename theArray =
		(
			local theFile = OpenFile theFilename 
			if theFile != undefined then
			(
				try ( while not eof theFile do append theArray (ReadLine theFile ) )catch()
				close theFile 
			)
		)
		
		fn CollectPools =
		(
			local tempArray = #()
			local result = waitForCommandToComplete "-pools " 5
			if result == #success then			
			(
				ReadFileIntoArray submitOutputFile tempArray
				ddl_PoolList.items = tempArray
				collectedPools = true
			)
			result
		)		
		
		fn CollectGroups =
		(
			local tempArray = #()
			local result = waitForCommandToComplete "-groups " 5
			if result == #success then			
			(
				ReadFileIntoArray submitOutputFile tempArray
				ddl_groupList.items = tempArray
				collectedPools = true
			)
			result
		)	
		
		fn getRenderMessage =
		(
			local resultFile = OpenFile submitOutputFile
			local renderMsg = "No message."
			if (resultFile != undefined) do
			(
				seek resultFile #eof
				local fileSize = filepos resultFile
				seek resultFile 0
				renderMsg = readChars resultFile fileSize errorAtEOF:false
				close resultFile
			)
			renderMsg 
		)		
		
		fn SubmitPartitionJobToDeadline = 
		(
			jobName = "STOKE SIM [RANGE "+(spn_partitionStart.value as string) + " TO " + spn_partitionEnd.value as string + " OF " + spn_partitionCount.value as string +" PARTS] "
			thePartitionCount = spn_partitionEnd.value - spn_partitionStart.value + 1
			
			local initialArgs = ""
			
			local TempMaxFile = GetDir #temp + "\\" +maxFileName
			if maxFileName == "" do TempMaxFile += "untitled.max"
			if (doesFileExist TempMaxFile) do deleteFile TempMaxFile
			
			saveMaxFile TempMaxFile clearNeedSaveFlag:false useNewFile:false quiet:true
			if not doesFileExist TempMaxFile do return #savefailed
			
			theFrameSequence = ""
			for i = spn_partitionStart.value to spn_partitionEnd.value - 1 do theFrameSequence+= i as string + ","
			theFrameSequence += spn_partitionEnd.value as string
				
			MAXScriptFile	=  StokeGlobalInterface.HomeDirectory + "Scripts\\StokePartitionTasksOnDeadline.ms"
				
			if ((CreateSubmitInfoFile SubmitInfofileName ) != true) do return #SubmitInfoFileCreationFailed
			if ((CreateJobInfoFile JobInfofileName ) != true) do return #JobInfoFileCreationFailed
			
			if not doesFileExist SubmitInfofileName do return #SubmitInfoFileMissing
			if not doesFileExist JobInfofileName do return #JobInfoFileMissing

			initialArgs += "\"" +SubmitInfofileName + "\" \"" + JobInfofileName  + "\" \"" + TempMaxFile  + "\" "
			initialArgs += "\"" + MAXScriptFile + "\" "	
			
			local retCode = waitForCommandToComplete initialArgs 3600
			
			LastMessage = getRenderMessage()
			local strstr = LastMessage as stringStream
			local txt = ""
			while not eof strstr do 
			(
				local theLine = (readline strstr)
				txt += theLine + "\n"
			)	
			
			theMessage = "Deadline Submission Report:\n" 
			theMessage += "Partitions Submitted: " + thePartitionCount as string  + "\n\n"
			theMessage += txt 
			format "%\n" txt
			MessageBox  theMessage title:"STOKE Deadline Submission"
		)
		
		on btn_SubmitToDeadline pressed do SubmitPartitionJobToDeadline()
		
		
		on btn_partition pressed do
		(
			if ddl_stokeObjects.selection > 0 and isValidNode theStokeObjects[ddl_stokeObjects.selection] do 
				partitionStokeObject theStokeObjects[ddl_stokeObjects.selection]
		)
		
		on btn_cancel pressed do
		(
			Stoke_Progress_Cancel = true
		)
		
		fn updateUI =
		(
			theStokeObjects = for o in geometry where classof o.baseobject == Stoke collect o
			ddl_stokeObjects.items = for o in theStokeObjects collect o.name
			updateButtonEnabledStates()
		)
		
		on ddl_stokeObjects selected itm do updateButtonEnabledStates()
		
		on btn_submitToDeadlineControls pressed do
		(
			submitToDeadlineControls = not submitToDeadlineControls
			if submitToDeadlineControls then
			(
				if not collectedPools do 
				(
					btn_submitToDeadlineControls.text = "Collecting Pools and Groups, PLEASE WAIT..."
					if (maxVersion())[1] > 12000 do windows.processPostedMessages()
					if CollectPools() == #success then 
					(
						CollectGroups()
						theVal = getIniSetting StokeIniPath "JobSettings"  "Pool" 
						theIndex = findItem ddl_poollist.items theVal
						if theIndex == 0 do theIndex = 1
						ddl_poollist.selection = theIndex
							
						theVal = getIniSetting StokeIniPath "JobSettings"  "Group" 
						theIndex = findItem ddl_grouplist.items theVal
						if theIndex == 0 do theIndex = 1
						ddl_grouplist.selection = theIndex						
					)
					else
						deadlineFound = false
				)
				STOKE_PartitioningDialog.height = 465
				btn_submitToDeadlineControls.text = "COLLAPSE Deadline Submission Controls... "
			)
			else
			(
				STOKE_PartitioningDialog.height =  235
				btn_submitToDeadlineControls.text = "EXPAND Deadline Submission Controls... "
			)
			updateButtonEnabledStates()
		)
		
		on btn_updateStokeList pressed do updateUI()
		
		on STOKE_PartitioningDialog  open do
		(
			updateUI()
			local theVal = execute (getIniSetting StokeIniPath "JobSettings"  "Priority")
			if theVal == OK do theVal = 50
			job_priority_update theVal
			
			theVal = getIniSetting StokeIniPath "JobSettings"  "UserName" 
			if theVal == "" do theVal = sysinfo.username
			edt_user.text = theVal	

			theVal = getIniSetting StokeIniPath "JobSettings"  "Department" 
			edt_dept.text = theVal	

			theVal = execute (getIniSetting StokeIniPath "JobSettings"  "MachineLimit")
			if theVal == OK do theVal = 0
			spn_machineLimit.value = theVal
			
			theVal = getIniSetting StokeIniPath "JobSettings" "NetworkPath" 
			if doesFileExist theVal and pathIsNetworkPath theVal do edt_networkPath.text = theVal
			updateButtonEnabledStates()
			
		)
	)--end rollout
	createDialog STOKE_PartitioningDialog 400  235
)--end script