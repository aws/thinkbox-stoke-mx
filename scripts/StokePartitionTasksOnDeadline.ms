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
	
	du.SetTitle "STOKE PARTITIONING..." --set the job title 
	du.LogMessage "Starting STOKE Partition Job..." --output a message to the log
	ETAstartTime = st = timestamp() --get the current system time
	
	partitionNumber = du.CurrentFrame
	thePartCount = execute (du.GetJobInfoEntry "TotalPartitions")
	theStokeObject = getNodeByName (du.GetJobInfoEntry "StokeObject")
	useThreadLimit = execute (du.GetJobInfoEntry "UseThreadLimit")
	local theThreadsCount = sysinfo.cpucount
	if useThreadLimit do theThreadsCount = execute (du.GetJobInfoEntry "ThreadLimit")
	theStokeObject.delegate.SetNumSerializerThreads  theThreadsCount
	du.LogMessage ( ">Saving using "+theThreadsCount  as string+" of "+ sysinfo.cpucount as string +" threads.") 
	
	if theStokeObject.delegate.SimulationMagma == undefined or theStokeObject.delegate.BirthMagma == undefined do
	(
		du.LogMessage ">Initializing Magma..."
		theStokeObject.delegate.InitSimMagmaHolder()
	)
	
	local oldSeed = theStokeObject.randomSeed
	
	du.LogMessage (">Current Partition Is " + partitionNumber as string + " of " + thePartCount as string)
	du.LogMessage ">Incrementing Seed..."
	theStokeObject.randomSeed += (partitionNumber-1) 
	
	
	
	theStokeObject.saveResultsToDisk = true
	local theCountString = thePartCount as string
	local networkPath = du.GetJobInfoEntry "NetworkPath"
	local outputPrefix = "STKPRT_"+theStokeObject.outputPrefix+"_part"+ (LeadingZeros partitionNumber theCountString.count)+ partitionNumber as string + "of"+theCountString+"_"
	local outPath = networkPath + "\\Stoke_" +theStokeObject.randomID+"\\" + theStokeObject.outputVersion + "\\"
	makeDir outPath all:true
	theStokeObject.delegate.InitializeParticleCache ( outPath+outputPrefix+"_####.prt" )	
	
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
	du.LogMessage ">Save Channel Map:" 
	for i in theChannelsToSaveMap do
		du.LogMessage ("  "+ i)
	
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
		( StokeGlobalInterface.CreateAdditiveVelocityField validFields ) 
	else
		( validFields[1] )
		
	theSim.SetVelocitySource theVelocityField
	
	theStokeObject.delegate.ResetParticleCache()
	
	if not theStokeObject.useViewportParticles do 
		StokeGlobalInterface.BeginRenderMode theParticleObjects
	
	local theCallback = StokePartCallback stokePlugin:theStokeObject stokeSim:theSim particleSources:theParticleSources velocityFields:theVelocityFields
	local result = theSim.Simulate frameCallback:(theCallback.onFrameUpdate)

	if not theStokeObject.useViewportParticles do 
		StokeGlobalInterface.EndRenderMode theParticleObjects

	du.LogMessage ">Flushing Cache..."
	local st2 = timestamp()
	theStokeObject.delegate.FlushParticleCache()
	du.LogMessage  (">Cache Flush Time : "+((timestamp()-st2)/1000.0) as string+"  sec." )
	
	du.LogMessage ">Decrementing Seed..."
	theStokeObject.randomSeed = oldSeed
	du.LogMessage (">Task Finished in "+ ((timestamp() - st)/1000.0) as string + " sec.") --output the task duration
	du.LogMessage ">Done."
	true 
)--end script
