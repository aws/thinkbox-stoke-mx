-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
MacroScript StokeFieldSimOnDeadline category:"Stoke" tooltip:"Stoke Field Simulation On Deadline." icon:#("Stoke",11)
(
	global STOKE_FieldSimDeadlineDialog
	try(destroyDialog STOKE_FieldSimDeadlineDialog)catch()
	
	global Stoke_Progress_Cancel = false
	global Stoke_PresetsArrowBitmap
	if Stoke_PresetsArrowBitmap == undefined do Stoke_PresetsArrowBitmap = bitmap 640 32
	
	local submitToDeadlineControls = false
	local collectedPools = false
	local deadlineVersion = #default
	local deadlineFound = true
	
	rollout STOKE_FieldSimDeadlineDialog "STOKE Field Simulation On Deadline"
	(
		local theStokeObjects = #()
		local StokeIniPath = GetDir #plugcfg + "//StokePreferences.ini"
		
		label lbl_stokeObjects "Select a STOKE Field Sim Object to Simulate On Deadline:" across:2 align:#left offset:[-5,-1]
		button btn_updateStokeList "Update List" align:#right offset:[7,-3] height:20 
		dropdownlist ddl_stokeObjects "" width:390 align:#left  offset:[-7,-5] --across:2 
		
		group "Options"
		(
			checkbox chk_useMemoryLimit "Override Memory Limit"  across:2 align:#left
			spinner spn_cacheMemory "Memory Limit in MB:"  type:#integer range:[0,65536,4096] fieldwidth:40 align:#right
			checkbox chk_overrideThreads "Override Max. Saving Threads" align:#left offset:[0,0] across:2
			spinner spn_MaxThreads "Max. Saving Threads:" range:[1,sysinfo.cpucount,1] type:#integer fieldwidth:40 align:#right offset:[0,0]
		)
		--progressbar prg_partitionsProgress height:8 color:red width:390 align:#center offset:[0,0]
		--progressbar prg_progress height:8 color:green width:390 align:#center offset:[0,-3]
		
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
			
			--spinner spn_machineLimit "Machine Limit:" range:[0,1000,0] type:#integer fieldwidth:35 offset:[0,0] align:#right tooltip:"This value defines the number of Deadline Workers that are allowed to process Tasks of the Partitioning Job at the same time.\n\nIt is a good idea to limit the number of Workers when saving very large PRT files over the network."

			--checkbox chk_useThreadLimitOnDeadline "Limit Number of Saving Threads" across:2 align:#left offset:[0,-2]
			--spinner spn_ThreadLimitOnDeadline "Max. Saving Threads:" range:[1,16,1] type:#integer fieldwidth:35 align:#right
			
		)
		
		button btn_SubmitToDeadline "SIMULATE ON DEADLINE" width:392 height:40 align:#center offset:[0,-3] enabled:false


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
				if deadlineFound then
				(	
					if doesFileExist edt_networkPath.text and pathIsNetworkPath edt_networkPath.text then 
					(
						btn_SubmitToDeadline.enabled = true
						btn_SubmitToDeadline.text = "SIMULATE ON DEADLINE"
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
				btn_SubmitToDeadline.text = "NEED A STOKE FIELD SIMULATION OBJECT TO SUBMIT"	
			)
		)
		
		
		on spn_priority changed value do setIniSetting StokeIniPath "JobSettings"  "Priority" ( (job_priority_update value) as string )
		on sld_priority clicked value do setIniSetting StokeIniPath "JobSettings"  "Priority" (  (job_priority_update value) as string )
		--on spn_machineLimit changed val do setIniSetting StokeIniPath "JobSettings"  "MachineLimit" (val as string)
		
		on edt_user entered txt do setIniSetting StokeIniPath "JobSettings"  "UserName" txt
		on edt_dept entered txt do setIniSetting StokeIniPath "JobSettings"  "Department" txt
		on ddl_poollist selected itm do setIniSetting StokeIniPath "JobSettings"  "Pool" ddl_poollist.selected
		on ddl_grouplist selected itm do setIniSetting StokeIniPath "JobSettings"  "Group" ddl_grouplist.selected
			
		on btn_getNetworkPath pressed do 
		(
			local thePath = getSavePath initialDir:edt_networkPath.text
			if thePath != undefined do 
			if pathIsNetworkPath thePath do
				setIniSetting StokeIniPath "JobSettings" "NetworkPath" (edt_networkPath.text = thePath)
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
				--for p = spn_partitionStart.value to spn_partitionEnd.value do
				(
					--local theCountString = spn_partitionCount.value as string
					local outputPrefix = "STKVDB_"+theStokeObject.outputPrefix
					local outPath = edt_networkPath.text + "\\StokeField_" +theStokeObject.randomID+"\\" + theStokeObject.outputVersion + "\\"
					local thePartPath = ( outPath+outputPrefix+"_####.vdb" )	
					format "OutputDirectory%=%\n" outputFilenameIndex	(getFilenamePath thePartPath) 	to:submitInfoFile
					format "OutputFilename%=%\n" outputFilenameIndex (filenameFromPath thePartPath) to:submitInfoFile
					outputFilenameIndex +=1
				)
				
				format "MachineLimit=0\n" to:submitInfoFile --spn_machineLimit.value 
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
				
				if( ((maxVersion())[1]/1000 as integer) >= 12 ) then
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
				--format "TotalPartitions=%\n" spn_partitionCount.value to:JobInfoFile
				format "StokeObject=%\n" ddl_stokeObjects.selected to:JobInfoFile

				format "UseThreadLimit=%\n" chk_overrideThreads.state to:JobInfoFile
				format "ThreadLimit=%\n" spn_MaxThreads.value to:JobInfoFile

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
			jobName = getFileNameFile maxFileName + " > " +ddl_stokeObjects.selected + " [STOKE FIELD SIM]"
			local initialArgs = ""
			
			local TempMaxFile = GetDir #temp + "\\" +maxFileName
			if maxFileName == "" do TempMaxFile += "untitled.max"
			if (doesFileExist TempMaxFile) do deleteFile TempMaxFile
			
			saveMaxFile TempMaxFile clearNeedSaveFlag:false useNewFile:false quiet:true
			if not doesFileExist TempMaxFile do return #savefailed
			
			theFrameSequence = "0"
			--for i = spn_partitionStart.value to spn_partitionEnd.value - 1 do theFrameSequence+= i as string + ","
			--theFrameSequence += spn_partitionEnd.value as string
				
			MAXScriptFile	=  StokeGlobalInterface.HomeDirectory + "Scripts\\StokeFieldSimTasksOnDeadline.ms"
				
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
			theMessage += txt 
			format "%\n" txt
			MessageBox  theMessage title:"STOKE Field Sim Deadline Submission"
		)
		
		on btn_SubmitToDeadline pressed do SubmitPartitionJobToDeadline()
		
		
		fn updateUI =
		(
			theStokeObjects = for o in geometry where classof o.baseobject == StokeFieldSIM collect o
			ddl_stokeObjects.items = for o in theStokeObjects collect o.name
			updateButtonEnabledStates()
		)
		
		on ddl_stokeObjects selected itm do updateButtonEnabledStates()
		
		on btn_updateStokeList pressed do updateUI()
		
		on STOKE_FieldSimDeadlineDialog  open do
		(
			updateUI()
			
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
			
			
			local theVal = execute (getIniSetting StokeIniPath "JobSettings"  "Priority")
			if theVal == OK do theVal = 50
			job_priority_update theVal
			
			theVal = getIniSetting StokeIniPath "JobSettings"  "UserName" 
			if theVal == "" do theVal = sysinfo.username
			edt_user.text = theVal	

			theVal = getIniSetting StokeIniPath "JobSettings"  "Department" 
			edt_dept.text = theVal	

			/*
			theVal = execute (getIniSetting StokeIniPath "JobSettings"  "MachineLimit")
			if theVal == OK do theVal = 0
			spn_machineLimit.value = theVal
			*/
			theVal = getIniSetting StokeIniPath "JobSettings" "NetworkPath" 
			if doesFileExist theVal and pathIsNetworkPath theVal do edt_networkPath.text = theVal
			updateButtonEnabledStates()
			
		)
	)--end rollout
	createDialog STOKE_FieldSimDeadlineDialog 400  310
)--end script