-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
struct StokeSimulator (
	private
	
	-- The objects which generate particles on each timestep of the simulation.
	particleSources = #(), 
	
	-- The object which generates velocity for the particles.
	velocitySource = undefined, 
	
	-- The object which produces ID values for new particles
	idSource = undefined, 
	
	-- The extra channels we want the particles to carry
	extraChannels = #(), 
	
	-- The particle set that stores the accumulated particles of the simulation
	currentParticles = undefined,
	
	-- The advection strategy for applying velocity to particles
	advector = ( StokeGlobalInterface.CreateAdvector "RK2" ), 
	
	-- Flag to enable/disable deleting particles who's age exceeds their lifespan
	deleteOldParticles = true, 
	
	-- The time to start the simulation at
	startTime = ( animationRange.start as time ), 
	
	-- The time to end the simulation at
	endTime = ( animationRange.end as time ),
	
	-- How many times per frame that the simulation is processed
	timeSubsteps = 1, 
	
	-- The current time of the simulation
	simTime = undefined, 
	
	-- The amount of time taken by the last frame's particle generation process
	lastGenerateTime = 0, 
	
	-- The amount of time taken by the last frame's advection process
	lastAdvectTime = 0, 
	
	-- The amount of time taken by the Magma Per Step evaluation (subset of the lastAdvectTime)
	lastMagmaStepTime = 0,
	
	-- The amount of time taken by the last frame's source objects update process
	lastUpdateTime = 0, 
	
	-- The amount of time taken by the last frame's dead particle deletion process
	lastDeleteTime = 0,

	simMagmaHolder = undefined,

	birthMagmaHolder = undefined,

	useMagma = false,

	errorReporter = undefined,

	stokeObject = undefined,
	
	-- Initializes the simulator's member variables when Simulate is called for the first time. This is not run when resuming a simulation
	fn InitSimulator = (
		simTime = startTime.frame as time
		
		idSource = StokeGlobalInterface.CreateIDAllocator()
		
		for pg in particleSources do (
			pg.IDAllocator = idSource
			pg.InitialVelocityField = velocitySource
		)
		
		currentParticles = StokeGlobalInterface.CreateParticleSet extraChannels
		
		OK
	),
	
	fn SimulateStep actualTime timeStepSeconds = (
		lastUpdateTime = timestamp()
		for pg in particleSources do (
			pg.UpdateToTime actualTime
		)
		velocitySource.UpdateToTime actualTime
		lastUpdateTime = timestamp() - lastUpdateTime

		StokeGlobalInterface.UpdateAgeChannel currentParticles timeStepSeconds
		
		lastGenerateTime = timestamp()
		for pg in particleSources do (
			pg.GenerateNextParticlesWithMagma currentParticles timeStepSeconds birthMagmaHolder errorReporter actualTime
		)
		lastGenerateTime = timestamp() - lastGenerateTime

		if not stokeObject.stopSimDueToMagmaError do (
			lastAdvectTime = timestamp()
			StokeGlobalInterface.UpdateAdvectionOffsetChannel currentParticles advector velocitySource timeStepSeconds
			if useMagma do
			(
				lastMagmaStepTime = timestamp()
				StokeGlobalInterface.MagmaEvaluateParticleSet currentParticles simMagmaHolder actualTime errorReporter
				lastMagmaStepTime = timestamp() - lastMagmaStepTime
			)
			StokeGlobalInterface.ApplyAdvectionOffsetChannel currentParticles
			lastAdvectTime = timestamp() - lastAdvectTime

			if deleteOldParticles do (
				lastDeleteTime = timestamp()
				currentParticles.DeleteDeadParticles()
				lastDeleteTime = timestamp() - lastDeleteTime
			)
		)
		
		OK
	),
	
	public
	
	totalGenerateTime = 0,
	totalAdvectTime = 0,
	totalMagmaStepTime = 0,
	totalUpdateTime = 0,
	totalDeleteTime = 0,
	
	-- Assign the time range to run the simulation over. Optionally specify a number of pre-roll frames, and the number simulation
	-- timesteps per frame.
	fn SetSimulationRange theStartTime theEndTime numSubSteps:1 = (
		-- Remove fractional frames from the start and end times
		startTime = (theStartTime as time).frame as time
		endTime = (theEndTime as time).frame as time
		
		if numSubSteps > 0 do timeSubsteps = (numSubSteps as integer)
		
		OK
	),
	
	-- Specify the channels that particles should carry through the simulation.
	fn SetParticleChannels theExtraChannels = (
		extraChannels = theExtraChannels
		
		OK
	),
	
	-- Adds a new particle source
	fn AddParticleSource theSource = (
		if simTime != undefined do throw "Cannot add a particle source to a running simulation" 
	
		-- TODO: Make sure this is a valid source!
		append particleSources theSource
		
		OK
	),
	
	-- Sets the velocity source, replacing any existing one
	fn SetVelocitySource theSource = (
		if simTime != undefined do throw "Cannot change the velocity source of a running simulation"
	
		-- TODO: Make sure this is a valid source!
		velocitySource = theSource
		
		OK
	),

	fn SetSimMagmaHolder theHolder = (
		simMagmaHolder = theHolder
	),

	fn SetBirthMagmaHolder theHolder = (
		birthMagmaHolder = theHolder
	),

	fn SetUseMagma use = (
		useMagma = use
	),

	fn SetErrorReporter reporter = (
		errorReporter = reporter
	),

	fn SetStokeObject object = (
		stokeObject = object
	),
	
	fn EnableParticleDeath enabled = (
		deleteOldParticles = enabled
	),
	
	-- Retrieves the time the simulation has run until. Will be undefined if the simulation was never started
	fn GetCurrentTime = (
		simTime
	),
	
	fn GetSimulationInterval = (
		interval startTime endTime
	),
	
	-- Retrieves the current set of particles that are the result of the simulation so far. Will be undefined if the simulation was never started.
	fn GetCurrentParticles = (
		currentParticles
	),
	
	-- Begins or resumes the simulation 
	fn Simulate frameCallback: = 
	(
		local returnValue = #success
		if particleSources.count == 0 or velocitySource == undefined then
		(
			returnValue = #error --throw "No particle sources added"
		)
		else
		(
			totalGenerateTime = 0
			totalAdvectTime = 0
			totalMagmaStepTime = 0
			totalUpdateTime = 0
			
			-- simTime should be undefined, unless we are continuing a partial simulation.
			if simTime == undefined do InitSimulator()
			
			local timeStepSeconds = (1.0 / FrameRate)/timeSubsteps
			
			-- Invoke the callback once before entering the main loop so we get progress before the first frame, or we can
			-- update the progress bar to the appropriate time when resuming a simulation.
			if frameCallback != unsupplied do 
			(
				local shouldContinue = frameCallback()
				-- Break out of the simulation if the callback returns false. This could be used to pause the simulator for example.
				if not shouldContinue do returnValue = #interrupted
			)
			while simTime < endTime and returnValue == #success do 
			(
				for i = 1 to timeSubsteps do 
				(
					local actualTime = simTime + ((i - 1) as float) / (timeSubsteps as float)
					at time actualTime
					(
						SimulateStep actualTime timeStepSeconds
						totalGenerateTime += lastGenerateTime
						totalAdvectTime += lastAdvectTime
						totalMagmaStepTime += lastMagmaStepTime
						totalUpdateTime += lastUpdateTime
						totalDeleteTime += lastDeleteTime
					)
				)
				
				-- We have moved the particle data through time to the next frame, so update it here.
				simTime += 1.0
				
				if frameCallback != unsupplied do 
				(
					local shouldContinue = frameCallback()
					-- Break out of the simulation if the callback returns false. This could be used to pause the simulator
					if not shouldContinue do returnValue = #interrupted
				)
			)--end while loop
		)--end else
		returnValue
	)
)