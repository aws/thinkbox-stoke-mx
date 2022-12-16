-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
global StokeFieldSimInit_ImplementationObject
struct StokeFieldSimInit_ImplementationObject
(
	fn getProductName = (#StokeField), --return the name of the product
	
	fn getProductTitle = ("StokeField"), --return the product title string 
	
	fn getOwnerType = 	(#baseobject), --return the type of the parent object (#baseobject, #modifiers, #renderelement)
	
	fn getGlobalInterface = (StokeFieldGlobalInterface), --return the Global Interface
	
	fn getHomeFolder = (StokeGlobalInterface.HomeDirectory), --return the Home directory of the product
	
	fn getCAName = 	("STOKE_FIELD_INIT_CA"), --return the name of the Custom Attribute to be generated to hold exposed parameters
	
	fn getMagmaFlowColors = "::Stoke_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("holder.initMagmaHolder"),
	
	fn GetExposedControlsTitle = ("Exposed in INITIAL STATE"), --return the title of the exposed parameters rollout
	
	fn getSupportsControlsExposure = (true), --returns trye if the implementation supports exposing controls to the Modifier Panel
	
	fn getSupportsDebug magmaNode = (false),	 --return true of the implementation supports debugging, false if it does not (e.g. early Ember, builds Render Elements)
	
	fn getSupportsUpdate magmaNode = (false), --returns True if the implementation supports Auto/Update controls, False if it does not (e.g. Stoke Particle and Field Simulators) 
	
	fn getSupportsParticleDebug magmaNode = (false),
	
	fn getRequires32BitChannels = (true), --return true if the object requires 32 bit channels (e.g. Stoke2), or false if 16 bit channels are acceptable (e.g. Krakatoa, Genome)
	
	fn getManagementFunctionsStructName = ("StokeFieldMFEditor_Functions"),
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false =
	(
		local theScript = (StokeGlobalInterface.HomeDirectory + "\\Scripts\\MagmaFlowEditor.ms") --the script to load
		::StokeFieldMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner StokeFieldSimInit_ImplementationObject theScript offscreen:offscreen
	),
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" = --returns the editor title base string
	(
		local txt = "STOKE FIELD SIM Magma Flow - "
		local theNodes = refs.dependentNodes magmaOwner
		for i in theNodes where i.baseobject == magmaOwner do
		(
			txt += "<"+i.name+">"	
		)
		txt += ".INITIAL"
		txt		
	),
	
	fn getUsedByRendererString theMagmaHolder theChannelName = 	("Stoke Field"), --returns the description string to display under a channel in the Editor
	
	fn getIterationMode magmaOwner = (#none), --return the Iteration mode if Genome, #none otherwise
	
	fn setIterationMode magmaOwner newMode = (false) --sets the iteration mode if Genome, or does nothing otherwise		
)

global StokeFieldSimSim_ImplementationObject
struct StokeFieldSimSim_ImplementationObject
(
	fn getProductName = (#StokeField),
	
	fn getProductTitle = ("StokeField"),	
	
	fn getOwnerType = (#baseobject),
	
	fn getGlobalInterface = (StokeFieldGlobalInterface),
	
	fn getHomeFolder = (StokeGlobalInterface.HomeDirectory),
	
	fn getCAName  = ("STOKE_FIELD_SIM_CA"),
	
	fn getMagmaFlowColors = "::Stoke_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("Holder.MagmaHolder"),
	
	fn GetExposedControlsTitle = ("Exposed in SIMULATION"),
	
	fn getSupportsControlsExposure = (true), 
	
	fn getSupportsDebug magmaNode = (false),	
	
	fn getSupportsUpdate magmaNode = (false),

	fn getSupportsParticleDebug magmaNode = (false),

	fn getRequires32BitChannels = (true),	
	
	fn getManagementFunctionsStructName = ("StokeFieldMFEditor_Functions"),	
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false  =
	(
		local theScript = (StokeGlobalInterface.HomeDirectory + "\\Scripts\\MagmaFlowEditor.ms")
		::StokeFieldMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner StokeFieldSimSim_ImplementationObject theScript offscreen:offscreen
	),	
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" =
	(
		local txt = "STOKE FIELD SIM Magma Flow - "
		local theNodes = refs.dependentNodes magmaOwner
		for i in theNodes where i.baseobject == magmaOwner do
		(
			txt += "<"+i.name+">"	
		)
		txt += ".SIMULATION"
		txt		
	),
	
	fn getUsedByRendererString theMagmaHolder theChannelName = 	("Stoke Field"),
	
	fn getIterationMode magmaOwner = (#none),
	
	fn setIterationMode magmaOwner newMode = (false)	
)

global StokeFieldStatic_ImplementationObject
struct StokeFieldStatic_ImplementationObject
(
	fn getProductName = (#StokeField),
	
	fn getProductTitle = ("StokeField"),	
	
	fn getOwnerType = (#baseobject),	
	
	fn getGlobalInterface = (StokeFieldGlobalInterface),
	
	fn getHomeFolder =	(StokeGlobalInterface.HomeDirectory),
	
	fn getCAName =	("STOKE_FIELD_STATIC_CA"),
	
	fn getMagmaFlowColors = "::Stoke_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("delegate.MagmaHolder"),
	
	fn GetExposedControlsTitle = ("Exposed Parameters"),
	
	fn getSupportsControlsExposure = (true), 
	
	fn getSupportsDebug magmaNode = (true),	
	
	fn getSupportsUpdate magmaNode = (true), 
	
	fn getSupportsParticleDebug magmaNode = (false),
	
	fn getRequires32BitChannels = (true),
	
	fn getManagementFunctionsStructName = ("StokeFieldMFEditor_Functions"),	
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false =
	(
		local theScript = (StokeGlobalInterface.HomeDirectory + "\\Scripts\\MagmaFlowEditor.ms")
		::StokeFieldMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner StokeFieldStatic_ImplementationObject theScript offscreen:offscreen
	),		
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" =
	(
		local txt = "STOKE FIELD Magma Flow - "
		local theNodes = refs.dependentNodes magmaOwner
		for i in theNodes where i.baseobject == magmaOwner do
		(
			txt += "<"+i.name+">"	
		)
		txt		
	),
	
	fn getUsedByRendererString theMagmaHolder theChannelName = ("Stoke Field"),
	
	fn getIterationMode magmaOwner =  (#none),
	
	fn setIterationMode magmaOwner newMode = (false 	)		
)

global GenomeModifier_ImplementationObject
struct GenomeModifier_ImplementationObject
(
	fn getProductName = (#genome	),
	
	fn getProductTitle = ("GENOME"),	
	
	fn getOwnerType = (#modifier),	
	
	fn getGlobalInterface = (GenomeGlobalInterface),
	
	fn getHomeFolder =	(GenomeGlobalInterface.GenomeHome),
	
	fn getCAName = ("KCM_CA"),
	
	fn getMagmaFlowColors = "::Genome_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("MagmaHolder"),	
	
	fn GetExposedControlsTitle = ("Exposed Parameters"),
	
	fn getSupportsControlsExposure = (true), 
	
	fn getSupportsDebug magmaNode = ( true ),	
	
	fn getSupportsUpdate magmaNode = (true), 	
	
	fn getSupportsParticleDebug magmaNode = (false),
	
	fn getRequires32BitChannels = (false),
	
	fn getManagementFunctionsStructName = ("GenomeMFEditor_Functions"),
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false =
	(
		local theScript = (GenomeGlobalInterface.GenomeHome + "\\Scripts\\MagmaFlowEditor.ms")
		::GenomeMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner GenomeModifier_ImplementationObject theScript offscreen:offscreen
	),			
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" =
	(
		local txt = "GENOME Magma Flow - "
		if magmaNode.readOnly do txt +="READ-ONLY - "
		txt += "<"+magmaOwner.name+">"	
		
		if currentPreset != "" do 
		(
			txt += "." + currentPreset
		)	
		
		txt		
	),
	fn getUsedByRendererString theMagmaHolder theChannelName =
	(
		"TriMesh Channel"
	),
	fn getIterationMode magmaOwner =
	(
		if not isDeleted magmaOwner then
		(
			if matchPattern magmaOwner.meshIterationPattern pattern:"Face Corners" then 
				#facevertex
			else 
				if matchPattern magmaOwner.meshIterationPattern pattern:"Faces" then #face else #vertex
		)
		else
			#none
	),
	fn setIterationMode magmaOwner newMode =
	(
		if not isDeleted magmaOwner then
		(
			magmaOwner.meshIterationPattern = case newMode of
			(
				default: "Vertices"
				#face: "Faces"
				#facevertex: "Face Corners"
			)
		)
	)
)

global StokeParticleSimSim_ImplementationObject
struct StokeParticleSimSim_ImplementationObject
(
	fn getProductName = (#StokeParticles),
	
	fn getProductTitle = ("StokeParticles"),	
	
	fn getOwnerType = (#baseobject),
	
	fn getGlobalInterface = (StokeFieldGlobalInterface),
	
	fn getHomeFolder = (StokeGlobalInterface.HomeDirectory),
	
	fn getCAName  = ("STOKE_PARTICLE_SIM_CA"),
	
	fn getMagmaFlowColors = "::Stoke_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("delegate.SimulationMagma"),
	
	fn GetExposedControlsTitle = ("Exposed in Per-Step Magma"),
	
	fn getSupportsControlsExposure = (false), 
	
	fn getSupportsDebug magmaNode = (false),	
	
	fn getSupportsUpdate magmaNode = (false), 	

	fn getSupportsParticleDebug magmaNode = (false),

	fn getRequires32BitChannels = (true),	
	
	fn getManagementFunctionsStructName = ("StokeFieldMFEditor_Functions"),	
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false  =
	(
		local theScript = (StokeGlobalInterface.HomeDirectory + "\\Scripts\\MagmaFlowEditor.ms")
		::StokeFieldMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner StokeParticleSimSim_ImplementationObject theScript offscreen:offscreen
	),	
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" =
	(
		local txt = "STOKE PARTICLE SIM Per-Step Magma Flow - "
		local theNodes = refs.dependentNodes magmaOwner
		for i in theNodes where i.baseobject == magmaOwner do
		(
			txt += "<"+i.name+">"	
		)
		txt += ".PER-STEP"
		txt		
	),
	
	fn getUsedByRendererString theMagmaHolder theChannelName = 	("Stoke Particles"),
	
	fn getIterationMode magmaOwner = (#none),
	
	fn setIterationMode magmaOwner newMode = (false)	
)

global StokeParticleSimBirth_ImplementationObject
struct StokeParticleSimBirth_ImplementationObject
(
	fn getProductName = (#StokeParticles),
	
	fn getProductTitle = ("StokeParticles"),	
	
	fn getOwnerType = (#baseobject),
	
	fn getGlobalInterface = (StokeFieldGlobalInterface),
	
	fn getHomeFolder = (StokeGlobalInterface.HomeDirectory),
	
	fn getCAName  = ("STOKE_PARTICLE_BIRTH_CA"),
	
	fn getMagmaFlowColors = "::Stoke_MagmaFlowColors",
	
	fn getPathToMagmaHolder = ("delegate.BirthMagma"),
	
	fn GetExposedControlsTitle = ("Exposed in Birth Magma"),
	
	fn getSupportsControlsExposure = (false), 
	
	fn getSupportsDebug magmaNode = (false),	
	
	fn getSupportsUpdate magmaNode = (false), 	

	fn getSupportsParticleDebug magmaNode = (false),

	fn getRequires32BitChannels = (true),	
	
	fn getManagementFunctionsStructName = ("StokeFieldMFEditor_Functions"),	
	
	fn openEditor theMagmaHolder theMagmaOwner offscreen:false  =
	(
		local theScript = (StokeGlobalInterface.HomeDirectory + "\\Scripts\\MagmaFlowEditor.ms")
		::StokeFieldMFEditor_Functions.OpenMagmaFlow theMagmaHolder theMagmaOwner StokeParticleSimBirth_ImplementationObject theScript offscreen:offscreen
	),	
	
	fn getEditorTitlePrefix magmaNode magmaOwner currentPreset:"" =
	(
		local txt = "STOKE PARTICLE SIM Birth Magma Flow - "
		local theNodes = refs.dependentNodes magmaOwner
		for i in theNodes where i.baseobject == magmaOwner do
		(
			txt += "<"+i.name+">"	
		)
		txt += ".BIRTH"
		txt		
	),
	
	fn getUsedByRendererString theMagmaHolder theChannelName = 	("Stoke Particles"),
	
	fn getIterationMode magmaOwner = (#none),
	
	fn setIterationMode magmaOwner newMode = (false)	
)