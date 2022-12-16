-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
	global du = DeadlineUtil  --this is the interface exposed by the Lightning Plug-in which provides communication between Deadline and 3ds Max
	if du == undefined do  --if the script is not being run on Deadline (for testing purposes),
	(
		struct DeadlineUtilStruct   --define a stand-in struct with the same methods as the Lightning plug-in
		(
			fn SetTitle title = ( format "Title: %\n" title ),
			fn SetProgress percent = (true),
			fn FailRender msg = ( throw msg ),
			fn GetJobInfoEntry key = ( undefined ),
			fn GetAuxFilename index = ( undefined ),
			fn LogMessage msg = ( format "%\n" msg ),
			CurrentFrame = ((sliderTime as string) as integer)
		)
		du = DeadlineUtilStruct() --create an instance of the stand-in struct
	)--end if
	
	fn LeadingZeros value count =
	(
		theStr = value as string
		substring "00000000000" 1 (count-(theStr.count))
	)
	
	fn onMagmaErrorCallbackFN LastInitErrorMsg NodeID: ExpressionID: =
	(
		du.FailRender ("Magma Error:" + LastInitErrorMsg + " NodeID:" + NodeID as string + " ExpressionID:" + ExpressionID as string)
	)
	
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
			du.LogMessage ("\t--Stoke Field Sim - Update at Time: "+ theTime.frame as string)
			StokeFieldSimPlugin.delegate.AddData StokeFieldSim theTime.frame
			true
		),
		
		fn FlushReadyCallback =
		(
			StokeFieldSimPlugin.delegate.SetSerializerCallback undefined
			StokeFieldSimPlugin.delegate.ResetCache()
		),
		
		fn FlushFrameReadyCallback theFile =
		(
			du.LogMessage  ("  --STOKE FIELD ASYNC DISK CACHE Saved File ["+ theFile + "]")
		)		
	)--end struct
	
	struct StokePartCallback
	(
		stokePlugin = undefined,
		stokeSim = undefined,
		
		particleSources = undefined,
		velocityFields = undefined,
		
		storageCounter = 0,
		updateCounter = 0,
		theMaxVersion = (maxVersion())[1],
		lastUpdate = 0,
		
		fn onFrameUpdate = 
		(
			local theTime = stokeSim.GetCurrentTime()
			if not stokePlugin.useCacheStartTime or theTime >= stokePlugin.CacheStartTime do
				stokePlugin.delegate.AddParticleSet ( stokeSim.GetCurrentParticles() ) theTime --this caches to memory and allows PRT saving 
				
			du.LogMessage ( "  >Processing Time "+ theTime as string + " : " + (((100.0 * (theTime-stokePlugin.startTime) / (stokePlugin.endTime-stokePlugin.startTime) ) )as string) +"%")
			if timestamp()-lastUpdate > 1000 do
			(
				du.SetProgress ((100.0 * (theTime-stokePlugin.startTime) / (stokePlugin.endTime-stokePlugin.startTime) ) )  
				lastUpdate = timestamp()
			)
			at time theTime 
			( 
				stokePlugin.updateParticleSourceRates particleSources
				stokePlugin.updateVelocitySourceScales velocityFields
			)
			true
		)
	)
	
	ETAstartTime = st = timestamp() --get the current system time
	theStokeObject = getNodeByName (du.GetJobInfoEntry "StokeObject")
	du.SetTitle ("STOKE FIELD SIMULATION - " + theStokeObject)  
	du.LogMessage ("Starting STOKE Field Simulation - " + theStokeObject)   --output a message to the log
	
	useThreadLimit = execute (du.GetJobInfoEntry "UseThreadLimit")
	local theThreadsCount = sysinfo.cpucount
	if useThreadLimit do theThreadsCount = execute (du.GetJobInfoEntry "ThreadLimit")
	theStokeObject.delegate.SetNumSerializerThreads  theThreadsCount
	du.LogMessage ( ">Saving using "+theThreadsCount  as string+" of "+ sysinfo.cpucount as string +" threads.") 
	
	theStokeObject.saveResultsToDisk = true
	local networkPath = du.GetJobInfoEntry "NetworkPath"
	local outputPrefix = "STKVDB_"+theStokeObject.outputPrefix
	local outPath = networkPath + "\\Stoke_" +theStokeObject.randomID+"\\" + theStokeObject.outputVersion + "\\"
	makeDir outPath all:true
	theStokeObject.delegate.ResetCache()	
	theStokeObject.delegate.SequencePath = ( outPath+outputPrefix+"_####.vdb" )	
	theStokeObject.delegate.UseDiskCache = true
	
	theSim = SimEmberFactory.CreateFieldCollection()
	theSim.SetSimulationRange theStokeObject.startTime theStokeObject.endTime numSubSteps:theStokeObject.subSteps
	theSim.SetInitMagmaExpression theStokeObject.holder.initMagmaHolder
	theSim.SetUpdateMagmaExpression theStokeObject.holder.magmaHolder
	theSim.SetGridParameters [theStokeObject.boundsMinX,theStokeObject.boundsMinY,theStokeObject.boundsMinZ] [theStokeObject.boundsMaxX, theStokeObject.boundsMaxY,theStokeObject.boundsMaxZ] theStokeObject.gridSpacing
	theSim.SetSolverEnabled (theStokeObject.usesolver>1)
	theSim.SetErrorCallback onMagmaErrorCallbackFN
				
	local theMemoryLimit = theStokeObject.memoryLimit
	local theFixedBufferSize = 512
	if theStokeObject.memoryLimit < 1024 do
	(
		theMemoryLimit = 1024
		theFixedBufferSize = 128
	)
	theStokeObject.delegate.SequenceCacheCapacityMB = theFixedBufferSize
	theStokeObject.delegate.SerializeQueueCapacityMB = theMemoryLimit - theFixedBufferSize
	
	local st = timestamp()
	local theCallback = StokeFieldSimCallback StokeFieldSimPlugin:theStokeObject StokeFieldSim:theSim
	local result = #success
	local errorMessage = ""
	
	try(
		result = theSim.Simulate frameCallback:(theCallback.onFrameUpdate)
	)catch(
		result = #error
		errorMessage = getCurrentException()
	)	
	
	du.LogMessage ">Flushing Cache..."
	local st2 = timestamp()
			
	try(theStokeObject.delegate.SetNumSerializerThreads saveThreads)catch()
	theStokeObject.isAsyncFlushActive = true
	theCallback.asyncCacheStart = timestamp()
	theStokeObject.delegate.FlushCacheAsync theCallback.FlushReadyCallback
	theStokeObject.delegate.SetSerializerCallback theCallback.FlushFrameReadyCallback
			
	du.LogMessage  (">Cache Flush Time : "+((timestamp()-st2)/1000.0) as string+"  sec." )
	
	du.LogMessage (">Task Finished in "+ ((timestamp() - st)/1000.0) as string + " sec.") --output the task duration
	du.LogMessage ">Done."
	true 
)--end script
