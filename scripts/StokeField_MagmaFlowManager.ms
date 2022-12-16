-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
(
--Search for
--#PRODUCT
--to find notes about modifying this script to fit different products
	
	global MagmaFlowEditor_CurrentEditors
	if MagmaFlowEditor_CurrentEditors == undefined do 
		MagmaFlowEditor_CurrentEditors = #()

	global MagmaFlowEditor_CurrentCurveEditors
	if MagmaFlowEditor_CurrentCurveEditors == undefined do 
		MagmaFlowEditor_CurrentCurveEditors = #()
	
	global MagmaFlowEditor_ClipboardArray
	if MagmaFlowEditor_ClipboardArray == undefined do 
		MagmaFlowEditor_ClipboardArray = #()
	
	global MagmaFlowEditor_ExportedControllerFunctions = false
	global MagmaFlowEditor_EditableBlopStack = #()
	global Krakatoa_ConvertMagmaVersion = true
	global Krakatoa_KMFRequiresExposureUpdate = false
	
	global MagmaFlowEditor_ClipboardExportedIDs = #()
	
	global Stoke_MagmaFlowColorsStruct
	struct Stoke_MagmaFlowColorsStruct 
	(
		HeaderColor = color 80 80 128,
		StandardSocketColor = color 0 0 150,
		HasDefaultSocketColor = color 0 255 0,
		HasStokeDefaultSocketColor = color 0 255 255,
		NeedsConnectionSocketColor = color 220 0 0,
		ErrorSocketColor = color 255 0 0,
		HighlightedSocketColor = color 255 200 0,
		IntegerSocketColor = color 100 0 100,
		FloatSocketColor = color 0 100 100,
		VectorSocketColor = color 0 140 240,
		OutputNode = color 200 255 200,
		InputVectorValueNode = color 200 240 255,
		InputFloatValueNode = color 200 240 220,
		InputIntValueNode = color 230 220 240,
		InputChannelNode = color 220 220 255,
		InputScriptNode= color 255 200 240,
		InputObjectNode  = color 220 255 240,
		InputGeometryNode  = color 255 220 220,
		InputParticlesNode  = color 255 255 200,
		InputTextureNode  = color 240 200 255,
		EditableBLOPNode  = color 220 200 255,
		EditableLoopNode = color 230 255 210,
		OperatorNode = color 255 225 200,
		FunctionOperatorNode = color 255 235 225,
		LogicOperatorNode = color 255 240 200,
		ObjectOperatorNode = color 255 200 200,
		DimmUnconnectedNode = color 200 200 200,
		MissingNode = color 255 150 150,
		StokeOperatorNode = color 255 200 225,
		GenomeOperatorNode = color 255 200 225
	)
	global Stoke_MagmaFlowColors = Stoke_MagmaFlowColorsStruct() --#PRODUCT
	 
	--GENERAL MANAGEMENT FUNCTIONS
	--#PRODUCT: Rename the functions below according to the product name:
	global StokeFieldMFEditor_Functions
	struct StokeFieldMFEditor_Functions
	(
		fn getNodeID magmaNode index =
		(
			local nodeArray = (magmaNode.GetNodes())
			if index == #last then 
				nodeArray[nodeArray.count]
			else if index > 0 and index <= nodeArray.count then
					nodeArray[index]
				else
					-1
		),
		fn getNodeCount magmaNode =
		(
			(magmaNode.GetNodes()).count
		),
		
		fn saveColorScheme theFileName =
		(
			local theFile = createFile theFileName
			local theArray = for p in getPropNames ::Stoke_MagmaFlowColors collect #(p,getProperty ::Stoke_MagmaFlowColors p) --#PRODUCT
			with PrintAllElements on format "global MagmaFlowColors_LoadScheme = %\n" theArray to:theFile
			close theFile
		),
		fn loadColorScheme theFileName =
		(
			global MagmaFlowColors_LoadScheme = #()
			if doesFileExist theFileName do
			(	
				try(fileIn theFileName)catch()
				for p in MagmaFlowColors_LoadScheme do
					try(setProperty Stoke_MagmaFlowColors p[1] p[2])catch()
			)
		),
		fn ResetColorScheme =
		(
			global Stoke_MagmaFlowColors = Stoke_MagmaFlowColorsStruct()
		),
		fn expandColorName theName =
		(
			local newName = ""
			for i = 1 to theName.count do
			(
				local theCode = bit.charAsInt theName[i]
				if i > 1 and (theCode > 64 and theCode < 91) do newName +=" "
				newName +=theName[i]
			)
			newName
		),
		fn createColorOptionsDialog theColors =
		(
			local txt = "" as stringStream
			global MagmaFlowEditor_ColorOptionsRollout
			format "rollout MagmaFlowEditor_ColorOptionsRollout \"MagmaFlow Color Options\" \n(\n" to:txt
			local theNames = (getPropNames theColors)
			for p in theNames do
			(
				format "colorPicker clr_% \"%\" color:(%) align:#right fieldwidth:50 height:18 offset:[0,-4] \n" (p as string) (StokeFieldMFEditor_Functions.expandColorName (p as string)) (getProperty theColors p) to:txt
				format "on clr_% changed val do ::MagmaFlowEditor_CurrentColorScheme.% = val\n " (p as string) (p as string) to:txt
			)
			format "button btn_OK \"OK\" width:105 height:30 across:2\n" to:txt
			format "button btn_cancel \"Cancel\" width:105 height:30 \n" to:txt
			format "on btn_ok pressed do (destroyDialog ::MagmaFlowEditor_ColorOptionsRollout)\n" to:txt
			format "on btn_cancel pressed do (::MagmaFlowEditor_CurrentColorScheme = undefined; destroyDialog ::MagmaFlowEditor_ColorOptionsRollout)\n" to:txt
			format ")\n" to:txt
			execute (txt as string)
		),

		fn purifyChannelName txt =
		(
			local newString = ""
			for i = txt.count to 1 by -1 do
			(
				local theCode = bit.charAsInt txt[i] 
				if (theCode > 47 and theCode < 58) or (theCode > 64 and theCode < 91) or (theCode > 94 and theCode < 123) then
					newString = txt[i] + newString
				else
					newString = "_" + newString
			)			
			local isDigit = true
			for i = 1 to txt.count while isDigit do
			(
				local theCode = bit.charAsInt txt[i] 
				if (theCode > 47 and theCode < 58) then
					newString[i] = "_"
				else
					isDigit = false
			)			
			if newString.count == 0 do newString = "Position"
			newString	
		),				
		fn setEditorProperty magmaNode PropName Val =
		(
			local thePropHash = (dotNetObject "System.String" PropName).GetHashCode()
			setAppData magmaNode thePropHash (Val as string)
		),
		fn getEditorProperty magmaNode PropName =
		(
			local thePropHash = (dotNetObject "System.String" PropName).GetHashCode()
			local theValue = getAppData magmaNode thePropHash
			if classof theValue == String and theValue.count >= 3 and bit.charasint theValue[1] == 239 and bit.charasint theValue[2] == 187 and bit.charasint theValue[3] == 191 do
			(
				theValue = substring theValue 4 -1
			)
			theValue
		),
		fn setUIProperty magmaNode ID PropName Val =
		(
			if ID != undefined AND ID >= 0 do
			(
				magmaNode.DeclareExtensionProperty ID PropName
				magmaNode.SetNodeProperty  ID PropName Val
			)
		),
		fn getNodeName magmaNode ID =
		(
			if ID != undefined AND ID >= 0 then
			(
				local theVal = magmaNode.GetNodeProperty ID "Name"
				if theVal == undefined then magmaNode.getNodeType ID else theVal
			)
			else
				""
		),
		fn getIndexByID magmaNode ID =
		(
			local nodeArray = magmaNode.GetNodes()
			local result=(for i = 1 to nodeArray.count where nodeArray[i] == ID collect i)[1]
			if result == undefined do result = 0
			result	
		),
		fn getUIProperty magmaNode ID PropName =
		(
			if ID != undefined AND ID >= 0 then
			(
				magmaNode.GetNodeProperty ID propName
			)
			else
				""
		),
		fn getConnectedNodes magmaNode ID =
		(
			if ID < 0 or magmaNode.getNodeType ID == "Output" do return #()
			local theResult = for c = 1 to magmaNode.getNumNodeOutputs ID collect #()
			
			for theID in magmaNode.GetNodes() where theID != ID do
			(
				local numSockets = magmaNode.getNumNodeInputs theID
				for c = 1 to numSockets do
				(
					theConnection = (magmaNode.getNodeInput theID c)
					if theConnection[1] == ID and theConnection[2] > 0 do 
					(
						if theResult[theConnection[2]] == undefined do theResult[theConnection[2]] = #()
						append theResult[theConnection[2]] #(theID,c)
					)
				)
			)
			theResult
		),
		
		fn getEventFromPFSource obj multiple:false=
		(
			global Magma_ParticleGroupsMultiple = multiple
			global Magma_PossibleParticleGroups = for o in refs.dependents obj where classof o == ParticleGroup AND isProperty o #name collect o
			global Magma_SelectedParticleGroup = undefined
			global Magma_SelectParticleGroup_Dialog
			if Magma_PossibleParticleGroups.count > 0 then
			(
				rollout Magma_SelectParticleGroup_Dialog "Select A Particle Flow Event As Source"
				(
					listbox lbx_particleGroups items:(for i in ::Magma_PossibleParticleGroups collect i.name) width:296 height:10 align:#center visible:(::Magma_ParticleGroupsMultiple==false)
					multilistbox mlb_particleGroups items:(for i in ::Magma_PossibleParticleGroups collect i.name) width:296 height:10 align:#center offset:[0,-140] visible:(::Magma_ParticleGroupsMultiple==true)
					button btn_ok "OK" width:146 height:30 align:#left across:2 offset:[-10,-3]
					button btn_cancel "Cancel" width:146 height:30 align:#right offset:[10,-3]
					fn onclosed =
					(
						if ::Magma_ParticleGroupsMultiple==true then
							::Magma_SelectedParticleGroup = for i in mlb_particleGroups.selection collect ::Magma_PossibleParticleGroups[i]
						else
							::Magma_SelectedParticleGroup = if lbx_particleGroups.selection > 0 then ::Magma_PossibleParticleGroups[lbx_particleGroups.selection] else undefined
						destroyDialog Magma_SelectParticleGroup_Dialog
					)
					on btn_ok pressed do onclosed()
					on btn_cancel pressed do destroyDialog Magma_SelectParticleGroup_Dialog
					on lbx_particleGroups doubleClicked itm do onclosed()
				)
				createDialog Magma_SelectParticleGroup_Dialog 300 180 modal:true
			)
			::Magma_SelectedParticleGroup
			--if Magma_PossibleParticleGroups.count > 0 then Magma_PossibleParticleGroups[1] else obj
		),		
		
		fn loadChannelsList magmaNode magmaOwner mode:#input source:#kcm iterationmode:#none =
		(
			case source of 
			(
				default:
				(
					channelsList = #(	
						#("Position", "float32", 3, "Particle Position"),
						#("Velocity", "float16", 3, "Particle Velocity units/sec."),
						#("Density", "float16", 1, "Particle Density"),
						#("Color", (if source==#kcm then "float16" else "float32"), 3 , "Scatter Color/Max Vertex Color"),
						#("Absorption", "float16", 3, "Absorption Color"),
						#("Emission", "float16", 3, "Emission Color"),
						#("Eccentricity", "float16", 1, "Shading:Phase Eccentricity"),
						#("SpecularPower", "float16", 1, "Phong Shading: Spec.Power"),
						#("SpecularLevel", "float16", 1, "Phong Shading: Spec.Level"),
						#("Normal", "float16", 3, "Normal Vector (X Axis)"),
						#("Tangent", "float16", 3, "Tangent Vector (Y Axis)"),
						#("TextureCoord", (if source==#kcm then "float16" else "float32"), 3, "3ds Max Mapping Channel 1"),
						#("Selection", "float32", 1, "Soft-Selection Weight"),
						#("Age", "float32", 1, "Particle Age"),
						#("LifeSpan", "float32", 1, "Particle Life Span"),
						#("NormalizedAge", "float32", 1, "Normalized Particle Age"),
						#("ID", "int32", 1, "Particle ID"),
						#("MtlIndex","int32", 1, "PFlow Particle Material ID"),
						#("MXSInteger", "int32", 1, "PFlow Scripted Integer Channel"),
						#("MXSFloat", "float32", 1, "PFlow Scripted Float Channel"),
						#("MXSVector", "float16", 3, "PFlow Scripted Vector Channel"),
						#("Orientation", "float16", 4, "Particle Quaternion Orientation"),
						#("Scale", "float16", 3, "Particle Scale"),
						#("Acceleration", "float16", 3, "Particle Acceleration"),
						#("SignedDistance", "float16", 1, "Distance To Geo.Surface"),
						#("SignedDistanceGradient", "float16", 3, "Distance Gradient"),
						#("ColorScalar", "float32", 3, "PRT Maker Color Scalar"),
						#("RandomValue", "float32", 1, "PRT Maker Random Value"),
						#("HairRoot", "float32", 3, "PRT Hair Root Pos."),
						#("HairLength", "float16", 1, "PRT Hair Length"),
						#("Distance", "float16", 1, "PRT Hair Dist. From Root"),
						#("ReferencePosition", "float32", 3, "PRT Hair Ref. Position"),
						#("SplineID", "int32", 1, "PRT Hair Spline ID"),
						#("DensityGradient", "float16", 3, "PRT FumeFX Density Gradient"),
						#("Fuel", "float16", 1, "FumeFX Fuel Channel"),
						#("Fire", "float16", 1, "FumeFX Fire Channel"),
						#("Temperature", "float16", 1, "FumeFX/RealFlow Temperature"),
						#("Force", "float32", 3, "RealFlow Force Channel"),
						#("Vorticity", "float32", 3, "RealFlow Vorticity Channel"),
						#("NeighborCount", "int32", 1, "RealFlow Neighbor Count"),
						#("IsolationTime", "float32", 1, "RealFlow Isolation Time"),
						#("Viscosity", "float32", 1, "RealFlow Viscosity"),
						#("Pressure", "float32", 1, "RealFlow Pressure"),
						#("Mass", "float32", 1, "RealFlow Mass"),
						#("Radius", "float16", 1, "Frost Radius"),
						#("ShapeIndex", "int16", 1, "Frost Shape Index"),
						#("GeomTime", "float16", 1, "Frost Geometry Time"),
						#("Droplet", "float16", 1, "Naiad Droplet Probability" ),
						#("AdvectionOffset", "float16", 3, "Stoke Particles Advection" )
					)	
					if mode == #input do append channelsList #("Index", "int32", 1, "MagmaFlow Particle Index")
					if mode != #input do append channelsList #("PRTViewportVector", "float16", 3, "Custom Vector Display in Viewports")
					if mode != #input do append channelsList #("PRTViewportColor", "float16", 3, "Custom Color Display in Viewports")
					if mode == #input and source == #relement do append channelsList #("Lighting", "float16", 3 , "Light Arriving At Particle")
					for i = 2 to 99 do append channelsList #("Mapping"+i as string, "float32", 3, "3ds Max Mapping Channel "+i as string)

					if Krakatoa_PresetsDirectory == undefined do 
						global Krakatoa_PresetsDirectory = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\Krakatoa"
					
					local theIniFile = (::Krakatoa_PresetsDirectory+"\\Krakatoa_CustomChannelsToSave.ini") 
					local customChannels = getIniSetting theIniFile
					for c in customChannels do
					(
						if findItem (for i in channelsList collect i[1]) c == 0 do
						(
							local theType = getIniSetting theIniFile c "Type"
							local theArity = getIniSetting theIniFile c "Arity"
							if theType != "" and theArity != "" do
							(
								append channelsList #(c,theType,theArity as integer,"User-Defined Channel")
							)
						)	
					)
				)
				#genome: 
				(
					channelsList = magmaOwner.GetChannelNames iterationmode forOutput:(mode==#output)
					if iterationmode == #facevertex do
					(
						for i = 2 to 99 do append channelsList #("Mapping"+i as string, "float32", 3, "3ds Max Mapping Channel "+i as string)	
					)
				)
			)
			channelsList
		),
		
		fn getFaceQueryChannels magmaNode magmaOwner =
		(
			local channelsList = #()
			if not isDeleted magmaOwner do
			(
				channelsList = for i in magmaOwner.GetChannelNames #facevertex forOutput:false collect i[1]
				for i in magmaOwner.GetChannelNames #face forOutput:false do appendIfUnique channelsList i[1]
				for i in magmaOwner.GetChannelNames #vertex forOutput:false do appendIfUnique channelsList i[1]
				for m = 2 to 99 do append channelsList ("Mapping"+(if m < 10 then "0" else "")+m as string)
			)
			channelsList
		),
		
		fn getVertexQueryChannels magmaNode magmaOwner =
		(
			local channelsList = #()
			if not isDeleted magmaOwner do
			(
				channelsList = for i in (magmaOwner.GetChannelNames #vertex forOutput:false) collect i[1]
				--for m = 2 to 99 do append channelsList ("Mapping"+(if m < 10 then "0" else "")+m as string)
			)
			channelsList
		),
		
		fn sortNamesAlphabetically v1 v2 =
		(
			if v1[1] > v2[1] then 1 else if v1[1] < v2[1] then -1 else 0
		),
		
		fn sortByUsage v1 v2 valArray: =
		(
			if valArray[v1] > valArray[v2] then -1 else if valArray[v1] < valArray[v2] then 1 else 0
		),		
		
		fn getSortingOptionsList source: =
		(
			case source of
			(
				default: 		#("Factory Default Order","By Usage Frequency","Alphabetical","Alphabetical, Mapping Last","Int>Float>Vector>Quat","Integers Only","Floats Only","Vectors Only","Quaternions Only","Mapping Only")
				#krakatoa: 	#("Factory Default Order","By Usage Frequency","Alphabetical","Alphabetical, Mapping Last","Int>Float>Vector>Quat","Integers Only","Floats Only","Vectors Only","Quaternions Only","Mapping Only","PRT Maker Channels Only","PRT Hair Channels Only","Frost Channels Only","FumeFX Channels Only","RealFlow Channels Only","Naiad Channels Only")
				#stokeField: 	#("Factory Default Order","By Usage Frequency","Alphabetical","Alphabetical, Mapping Last","Int>Float>Vector>Quat","Integers Only","Floats Only","Vectors Only","Quaternions Only","Mapping Only","PRT Maker Channels Only","PRT Hair Channels Only","Frost Channels Only","FumeFX Channels Only","RealFlow Channels Only","Naiad Channels Only")
			)
		),
		
		fn getSortedChannelsList channelsList sortOrder iniFolder includeCustom:false nodeType:#input filterPattern:"" =
		(
			local sortedChannelsList = #()
			local tempArray = if includeCustom then #("[Custom Channel]") else #()
			case sortOrder of
			(
				default: (
								sortedChannelsList = deepCopy channelsList
								join tempArray (for i in channelsList collect i[1])
							)
				2: (
						sortedChannelsList = deepCopy channelsList
						--qsort sortedChannelsList StokeFieldMFEditor_Functions.sortNamesAlphabetically --should not sort alphabetically since we want factory order when no usage data is available!
						local theIndexArray = for i = 1 to sortedChannelsList.count collect i
						local theUsageIniFile = if nodeType == #input then (iniFolder+"MagmaFlowInputChannelsUsageHistory.ini") else (iniFolder+"MagmaFlowOutputChannelsUsageHistory.ini")
						local theUsageList = for i in sortedChannelsList collect
						(
							theVal = execute (getIniSetting theUsageIniFile "UsageHistory" i[1])
							if theVal == OK do theVal = 0
							theVal
						)
						qsort theIndexArray StokeFieldMFEditor_Functions.sortByUsage valArray:theUsageList
						sortedChannelsList = for i in theIndexArray collect sortedChannelsList[i]
						join tempArray (for i in sortedChannelsList collect i[1])
					)
				3: (
						
						local sortedChannelsList = deepCopy channelsList
						qsort sortedChannelsList StokeFieldMFEditor_Functions.sortNamesAlphabetically
						join tempArray (for i in sortedChannelsList collect i[1])
					)
				4: (
						local notMappingArray = (for i in channelsList where (not matchPattern i[1] pattern:"Mapping*") collect i) 
						qsort notMappingArray  StokeFieldMFEditor_Functions.sortNamesAlphabetically
						local MappingArray = (for i in channelsList where (matchPattern i[1] pattern:"Mapping*") collect i) 
						qsort MappingArray  StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = join notMappingArray MappingArray
						join tempArray (for i in sortedChannelsList collect i[1])
					)
				5: (--int,float,vector,quat
						local intsArray = (for i in channelsList where (matchPattern i[2] pattern:"int*") collect i) 
						local floatsArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 1 ) collect i) 
						local vectorsArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 3 ) collect i) 
						local quatsArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 4 ) collect i) 
						qsort intsArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						qsort floatsArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						qsort vectorsArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						qsort quatsArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = #()
						join sortedChannelsList intsArray 
						join sortedChannelsList floatsArray 
						join sortedChannelsList vectorsArray 
						join sortedChannelsList quatsArray 
						join tempArray (for i in sortedChannelsList collect i[1])						
					)
				6: (--int
						local filterArray = (for i in channelsList where (matchPattern i[2] pattern:"int*") collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])						
					)					
				7: (--float
						local filterArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 1 ) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])						
					)		
				8: (--vector
						local filterArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 3 ) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])						
					)	
				9: (--quat
						local filterArray = (for i in channelsList where (matchPattern i[2] pattern:"float*" and i[3] == 4 ) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])						
					)	
				10: (--mapping only
						local filterArray = (for i in channelsList where ((matchPattern i[1] pattern:"Mapping*") or (findItem #("TextureCoord","Color") i[1] > 0)) collect i) 
						--qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)
				11:(--prt Maker
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*PRT Maker*") or (findItem #("Position","Color","Density") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)					
				12:(--prt hair
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*PRT Hair*") or (findItem #("Position","Color","Density","Normal","Tangent","TextureCoord") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)					
				13:(--frost
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*Frost*") or (findItem #("Position","Color","MtlIndex","Normal","Orientation","TextureCoord") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)					
				14:(--fumefx
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*Fume*") or (findItem #("Position","Emission","Density","Velocity","Normal","TextureCoord") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)
				15: (--realflow
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*Realflow*") or (findItem #("Position","Density","Velocity","TextureCoord") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)
				16: (--Naiad
						local filterArray = (for i in channelsList where ((matchPattern i[4] pattern:"*Naiad*") or (findItem #("Position","Velocity","Age","ID","Color","Density") i[1] > 0)) collect i) 
						qsort filterArray StokeFieldMFEditor_Functions.sortNamesAlphabetically
						sortedChannelsList = deepCopy filterArray
						join tempArray (for i in sortedChannelsList collect i[1])							
				)
			)
			tempArray = for i in tempArray where matchPattern i pattern:("*"+filterPattern+"*") collect i
			tempArray
		),		
		
		fn isChannelFloat magmaNode magmaOwner theChannelName =
		(
			local channelsList = StokeFieldMFEditor_Functions.loadChannelsList magmaNode magmaOwner
			local theResult = for i in channelsList where i[1] == theChannelName collect i
			if theResult.count == 1 then 
				matchPattern theResult[1][2] pattern:"float*" and theResult[1][3] == 1
			else 
				false
		),
		
		fn isChannelInteger magmaNode magmaOwner theChannelName =
		(
			local channelsList = StokeFieldMFEditor_Functions.loadChannelsList magmaNode magmaOwner
			local theResult = for i in channelsList where i[1] == theChannelName collect i
			if theResult.count == 1 then 
				matchPattern theResult[1][2] pattern:"int*" and theResult[1][3] == 1
			else 
				false
		),
		
		fn loadControlCurves crv_CurveControl theCI =
		(
			local theCount = theCI.getNumPoints()
			crv_CurveControl.curves[1].numPoints = theCount
			crv_CurveControl.curves[1].animatable  = false
			crv_CurveControl.curves[1].color = blue
			crv_CurveControl.curves[1].width = 2
			local theDomain = theCI.getDomain()
			local theMin = theDomain.x
			local theRange = theDomain.y - theDomain.x
			
			for i = theCount to 1 by -1 do
			(
				local cp = crv_CurveControl.curves[1].points[i]

				local theVal = theCI.getPointPos i
				theVal.x = theVal.x * theRange + theMin
				cp.value = theVal 
				local inTangent = theCI.getPointIn i
				inTangent.x = inTangent.x * theRange 
				cp.inTangent = inTangent
				local outTangent = theCI.getPointOut i
				outTangent.x = outTangent.x * theRange 
				cp.outTangent = outTangent
				local theFlags = (theCI.getPointFlags i)
				--format "theFlags = %\n" theFlags
				cp.bezier = bit.get theFlags 1
				cp.corner = bit.get theFlags 2 --true ---
				cp.lock_x = false 
				cp.lock_y = false
				cp.selected = lastPoint == i
				crv_CurveControl.curves[1].points[i] = cp
			)
			zoom crv_CurveControl #all			
		),		
		
		fn storeSettingsInMagmaHolder theHolder theSettings =
		(
			local theSettingsArray = for p in getPropNames theSettings collect #(p, getProperty theSettings p)
			with PrintAllElements on setAppData theHolder 20110719 (theSettingsArray as string)
		),
		
		fn restoreSettingsFromMagmaHolder theHolder =
		(
			local theValue = getAppData theHolder 20110719
			if classof theValue == String and theValue.count >= 3 and bit.charasint theValue[1] == 239 and bit.charasint theValue[2] == 187 and bit.charasint theValue[3] == 191 do
			(
				theValue = substring theValue 4 -1
			)
			theValue
		),
		
		
		fn exportCurrentLevelConnections theIndicesToProcess theMagmaHolder txt isClipboard:false editorID:""=
		(
			--format "-------------NODE CONNECTIONS---------------\n" to:txt
			
			local nodeArray = theMagmaHolder.GetNodes()
			
			for i in theIndicesToProcess do 
			(
				local id = nodeArray[i]
				for c = 1 to (theMagmaHolder.getNumNodeInputs id) do
				(
					local theInput = theMagmaHolder.getNodeInput id c
					if theInput[1] >= 0 then
					(
						if theMagmaHolder.getNodeType theInput[1] == "BLOPSocket" or matchPattern (theMagmaHolder.getNodeType theInput[1]) pattern:"*Loop*__Input" then
							format "magmaNode.setNodeInput node% % (magmaNode.GetNodeInput (magmaNode.CurrentBLOP) %)[1] %\n" id c theInput[2] theInput[2] to:txt 
						else
						(
							if findItem ::MagmaFlowEditor_ClipboardExportedIDs theInput[1] == 0 and isClipboard then
							(
								format "if ::magmaClipBoardId == \"%\" and ::magmaClipBoardPreserveWires == true then \n" editorID to:txt
									format "try(magmaNode.setNodeInput node% % (if magmaNode.getNodeType % != undefined then % else -1) %)catch()\n" id c theInput[1] theInput[1] theInput[2] to:txt
								format "else\n" to:txt
									format "try(magmaNode.setNodeInput node% % node% %)catch()\n" id c theInput[1] theInput[2] to:txt
							)
							else
								format "try(magmaNode.setNodeInput node% % node% %)catch()\n" id c theInput[1] theInput[2] to:txt
						)
					)
					else
						format "magmaNode.setNodeInput node% % -1 1 \n" id c to:txt
				)
			)			
		),
		
		fn exportControllerKeysFunction txt =
		(
			format "fn applyKeysToController controller keys = (for k in keys do (newKey = addNewKey controller k[1]; newKey.value = k[2]; newKey.inTangent = k[3]; newKey.outTangent = k[4]; newKey.inTangentType = k[5]; newKey.outTangentType = k[6]; newKey.inTangentLength = k[7]; newKey.outTangentLength = k[8]; newKey.freeHandle = k[9]; newKey.x_locked = k[10]; newKey.y_locked = k[11]; newKey.z_locked = k[12]; newKey.constantVelocity = k[13]))\n" to:txt
			::MagmaFlowEditor_ExportedControllerFunctions = true
		),
		
		fn exportNode theMagmaHolder i id txt =
		(
			local type = theMagmaHolder.GetNodeType id
			format "node% = magmaNode.createNode \"%\" \n" id type to:txt
			local numInputs = (theMagmaHolder.getNumNodeInputs id)
			format "magmaNode.setNumNodeInputs node% % \n" id numInputs to:txt
			format "magmaNode.setNumNodeOutputs node% % \n" id (theMagmaHolder.getNumNodeOutputs id) to:txt
			
			local thePropCount = (theMagmaHolder.GetNodePropertyNames id #internal).count 
			local theInternalPropNamesArray = (theMagmaHolder.GetNodePropertyNames id #internal)
			for p = 1 to thePropCount do
			( 
				local propName = theInternalPropNamesArray[p]
				if theMagmaHolder.GetNodePropertyReadOnly id propName do continue
				local propValue = theMagmaHolder.getNodeProperty id propName
				if try(classof propValue == String)catch(false) then
				(
					format "magmaNode.setNodeProperty node% \"%\" \"%\"\n" id propName propValue to:txt
				)
				/*else if (matchPattern propName pattern:"object" or matchPattern propName pattern:"node") and isValidNode propValue then
				(
					format "magmaNode.setNodeProperty node% \"%\" (getNodeByName \"%\")\n" id propName propValue.name to:txt
				)*/
				else if (matchPattern propName pattern:"objects" or matchPattern propName pattern:"nodes") then
				(
					local theArrayString = "" as stringStream
					format "#(" to:theArrayString
					local theNodes = for n in propValue where isValidNode n collect n
					for n = 1 to theNodes.count where isValidNode theNodes[n] do 
					(
						format "getNodeByName \"%\"" theNodes[n].name to:theArrayString
						if n < theNodes.count do 
							format "," to:theArrayString
					)
					format ")" to:theArrayString
					format "magmaNode.setNodeProperty node% \"%\" %\n" id propName (theArrayString as string) to:txt
				)			
				else if propName == "object" or propName == "node" then
				(
					if isValidNode propValue do 
						format "magmaNode.setNodeProperty node% \"%\" (getNodeByName \"%\")\n" id propName propValue.name to:txt
				)
				else if propName == "controller" then
				(
					format "ctrl=%(); ctrl.value = %\n" (classof propValue) (propValue.value) to:txt
					format "magmaNode.setNodeProperty node% \"%\" ctrl\n" id propName to:txt
					
					if propValue.numsubs == 0 then
					(
						local theKeys = for k in propValue.keys collect #(k.time, k.value, k.inTangent, k.outTangent, k.inTangentType, k.outTangentType, k.inTangentLength, k.outTangentLength, k.freeHandle, k.x_locked, k.y_locked, k.z_locked, k.constantVelocity  )
						if theKeys.count > 0 do
						(
							if not ::MagmaFlowEditor_ExportedControllerFunctions do StokeFieldMFEditor_Functions.exportControllerKeysFunction txt
							with PrintAllElements on format "applyKeysToController ctrl %\n" theKeys to:txt
						)
					)
					else
					(
						for c = 1 to propValue.numsubs do
						(
							local theKeys = for k in propValue[c].controller.keys collect #(k.time, k.value, k.inTangent, k.outTangent, k.inTangentType, k.outTangentType, k.inTangentLength, k.outTangentLength, k.freeHandle, k.x_locked, k.y_locked, k.z_locked, k.constantVelocity )
							if theKeys.count > 0 do
							(
								if not ::MagmaFlowEditor_ExportedControllerFunctions do StokeFieldMFEditor_Functions.exportControllerKeysFunction txt
								with PrintAllElements on format "applyKeysToController ctrl[%].controller %\n" c theKeys to:txt
							)
						)
					)
				)
				else if propName == "curve" then
				(
					local numCurvePoints = (propValue.getNumPoints())
					format "local theInterface = magmaNode.getNodeProperty node% \"%\" \n" id propName to:txt
					format "theInterface.setNumPoints %\n" numCurvePoints to:txt
					for j = 1 to numCurvePoints do
					(
						format "theInterface.setPoint % % % % %\n" j (propValue.getPointPos j) (propValue.getPointIn j) (propValue.getPointOut j) (propValue.getPointFlags j) to:txt
					)
				)
				else if propName == "enabled" then
					if propValue != true do format "magmaNode.setNodeProperty node% \"%\" %\n" id propName propValue to:txt
				else if propName == "texmap" then
				(
					if propValue != undefined do
					(
						format "try(theMaps = getClassInstances % \n" (classof propValue) to:txt
						format "for m in theMaps where m.name == \"%\" do magmaNode.setNodeProperty node% \"%\" m)catch() \n" propValue.name id propName to:txt
					)
				)
				else if classof propValue == Array then 
					with PrintAllElements on (format "magmaNode.setNodeProperty node% \"%\" %\n" id propName (propValue as string) to:txt)
				else
					format "magmaNode.setNodeProperty node% \"%\" %\n" id propName propValue to:txt
			)
			
			for j = 1 to numInputs do
			(
				local propDefault = theMagmaHolder.GetNodeInputDefaultValue id j
				if propDefault != undefined do
					format "magmaNode.setNodeInputDefaultValue node% % %\n" id j propDefault to:txt
			)			
			
			local listOfExtProps = theMagmaHolder.GetNodePropertyNames id #custom
			if listOfExtProps == undefined do listOfExtProps = #()
			local defaultPropsToSkip = #("Selected","Collapsed","HideUnconnectedOutputs","Notes")
			local defaultValuesToSkip = #(false,false,false,"")
			
			for p in listOfExtProps where not matchPattern p pattern:"Internal_*" do
			(
				local theProp =  (theMagmaHolder.getNodeProperty id p)
				if theProp != undefined do
				(
					local skipIndex = findItem defaultPropsToSkip p
					if skipIndex == 0 or defaultValuesToSkip[skipIndex] != theProp do
					(
						--format "Processing Property % % %\n" id p theProp
						if p == "Notes" do theProp = StokeFieldMFEditor_Functions.EscapeCharacters theProp
						format "magmaNode.DeclareExtensionProperty node% \"%\"\n" id p to:txt
						if classof theProp == String then 
							format "magmaNode.SetNodeProperty node% \"%\" \"%\"\n" id p theProp to:txt
						else 
							with PrintAllElements on format "magmaNode.SetNodeProperty node% \"%\" %\n" id p theProp to:txt
					)
				)
			)		
		),
		
		fn EscapeCharacters theString =
		(
			theNewString = ""
			for i = 1 to theString.count do
			(
				if findString "\"\\" theString[i] != undefined do theNewString+= "\\"
				if theString[i] == "\n" then
					theNewString+="\\n"
				else if theString[i] == "\t" then
					theNewString+="\\t"
				else
					theNewString+=theString[i]
			)
			theNewString
		),
		
		fn exportCurrentLevel theMagmaHolder theIndicesToProcess txt =
		(
			local nodeArray = theMagmaHolder.GetNodes()
			for i in theIndicesToProcess do 
			(
				local id = nodeArray[i]
				append ::MagmaFlowEditor_ClipboardExportedIDs id
				StokeFieldMFEditor_Functions.exportNode theMagmaHolder i id txt 
				if theMagmaHolder.IsNodeContainer id do
				(
					format "--------------------------------------------\n" to:txt
					theMagmaHolder.PushEditableBLOP id
					format "magmaNode.PushEditableBLOP node% \n" id to:txt
					
					--format "MagmaFlowEditor_EditableBlopStack = %\n" MagmaFlowEditor_EditableBlopStack
					
					if findItem MagmaFlowEditor_EditableBlopStack id > 0 do format "append ::MagmaFlowEditor_EditBLOPHistory node%\n" id to:txt
					local theBlopNodes = theMagmaHolder.GetNodes()
					local theBLOPIndicesToProcess = for j = 1 to theBlopNodes.count where findItem #(#BlopSocket,#BlopOutput) ((theMagmaHolder.getNodeType theBlopNodes[j]) as name) == 0 and not matchPattern ((theMagmaHolder.getNodeType theBlopNodes[j])) pattern:"*__Input" and not matchPattern ((theMagmaHolder.getNodeType theBlopNodes[j])) pattern:"*__Output" collect j
					StokeFieldMFEditor_Functions.exportCurrentLevel theMagmaHolder theBLOPIndicesToProcess txt 
					StokeFieldMFEditor_Functions.exportCurrentLevelConnections theBLOPIndicesToProcess theMagmaHolder txt 
					
					local numContainerOutputs = theMagmaHolder.GetNumNodeOutputs (theMagmaHolder.CurrentBLOP)
					if theMagmaHolder.getNodeType (theMagmaHolder.CurrentBLOP) == "Loop" do numContainerOutputs+= 1--add the implicit condition which is not counted!
					for j = 1 to numContainerOutputs do
					(
						local theOutput = (theMagmaHolder.GetOutput j)
						--if theOutput[1] >= 0 do 
						if theMagmaHolder.getNodeType theOutput[1] == "BLOPSocket" or matchPattern (theMagmaHolder.getNodeType theOutput[1]) pattern:"*Loop*__Input" then
							format "magmaNode.setOutput % (magmaNode.GetNodeInput (magmaNode.CurrentBLOP) %)[1] %\n" j theOutput[2] theOutput[2] to:txt
						else if theOutput[1] > -1 then
							format "magmaNode.setOutput % node% %\n" j theOutput[1] theOutput[2] to:txt
						else
							format "magmaNode.setOutput % -1 1\n" j to:txt
					)
					format "magmaNode.PopEditableBLOP() \n" to:txt
					theMagmaHolder.PopEditableBLOP()
				)--end if BLOP 
				format "--------------------------------------------\n" to:txt
			)			
		),
		
		fn emitMAXScriptCode theMagmaHolder theMagmaOwner ImpObject specificNodes: isUndo:false isClipboard:false editorID:"" thePresetName:"" isBlop:false=
		(
			::MagmaFlowEditor_ClipboardExportedIDs = #()
			::MagmaFlowEditor_ExportedControllerFunctions = false
			local txt = "" as stringStream
			format "(--MAGMAFLOW2--\n" to:txt
			format "global MagmaFlowEditor_EditBLOPHistory = #()\n" to:txt
			if not isDeleted theMagmaOwner and classof theMagmaOwner == GenomeModifier do
			(
				local thePropertiesArray = for p in getPropNames theMagmaOwner where findItem #(#ThreadingEnabled, #magmaHolder, #MeshIterationPattern) p == 0 collect #(p, getProperty theMagmaOwner p)
				with PrintAllElements on 
					format "global MagmaFlowEditor_Genome_Properties = %\n" thePropertiesArray to:txt

				format "global MagmaFlowEditor_Genome_MeshIterationMode = %\n" (ImpObject.getIterationMode theMagmaOwner) to:txt
				if thePresetName != "" do format "global MagmaFlowEditor_Genome_PresetName = \"%\"\n" thePresetName to:txt
			)
			
			if not isClipboard and not isBlop do format "magmaNode.note=\"%\"\n" (substituteString theMagmaHolder.note "\n" "\\n\\r") to:txt
			
			--format "-------------NODE DEFINITIONS---------------\n" to:txt
			
			local nodeCount = (theMagmaHolder.GetNodes()).count 
			local theIndicesToProcess = for i = 1 to nodeCount collect i
			
			if specificNodes != unsupplied do theIndicesToProcess = specificNodes
			StokeFieldMFEditor_Functions.exportCurrentLevel theMagmaHolder theIndicesToProcess  txt 
			StokeFieldMFEditor_Functions.exportCurrentLevelConnections theIndicesToProcess theMagmaHolder txt isClipboard:isClipboard editorID:editorID
			format "--------------------------------------------\n" to:txt	
			if isUndo do format "for i in ::MagmaFlowEditor_EditBLOPHistory do magmaNode.pushEditableBLOP i\n" to:txt 
			format ")\n" to:txt
			txt as string
		),
		
		fn convertKMFNode theMagmaHolder theNodesList theNodeDef theNodeIndex theParrentArray:#() theMaterialLibrary: =
		(
			--HANDLING OUTPUT NODE
			if theNodeDef[1] == "Output" do
			(
				theNewNode = (theMagmaHolder.createNode "Output")
				theMagmaHolder.setNodeProperty theNewNode "channelName" theNodeDef[3][1]
				theMagmaHolder.setNodeProperty theNewNode "channelType" (theNodeDef[3][2]+"["+theNodeDef[3][3] as string+"]")
			)
			
			--HANDLING INPUT NODES
			if theNodeDef[1] == "Input" do 
			(
				if theNodeDef[3][1] == "Channel" do
				(
					theNewNode = theMagmaHolder.createNode "InputChannel"
					theMagmaHolder.setNodeProperty theNewNode "channelName" theNodeDef[3][2]
				)
				if theNodeDef[3][1] == "TextureMap" do
				(
					theNewNode = theMagmaHolder.createNode "InputTexmap"
					if theMaterialLibrary != undefined do
					(
						theMap = (for m in theMaterialLibrary collect m)[theNodeDef[3][2]-1]
						if theMap != undefined do 
							theMagmaHolder.setNodeProperty theNewNode "texmap" theMap
					)
					if classof theNodeDef[3][3] == String do 
						try(theMagmaHolder.setNodeProperty theNewNode "resultType" theNodeDef[3][3])catch()
				)
				if theNodeDef[3][1] == "Geometry" do
				(
					theNewNode = theMagmaHolder.createNode "InputGeometry"
					local theGeoNodesList = for aGeoNode in theNodeDef[3][2] where isValidNode (theGeoNode = (getNodeByName aGeoNode)) collect theGeoNode
					theMagmaHolder.setNodeProperty theNewNode "nodes" theGeoNodesList
				)
				if theNodeDef[3][1] == "Script" do
				(
					theNewNode = theMagmaHolder.createNode "InputScript"
					theMagmaHolder.setNodeProperty theNewNode "script" theNodeDef[3][2]
				)				
				
				if theNodeDef[3][1] == "Value" do
				(
					theNewNode = theMagmaHolder.createNode "InputValue"
					theCtrl = try((execute theNodeDef[3][3]))catch(undefined)
					if theCtrl == undefined then 
					(
						case theNodeDef[3][2] of
						(
							"Integer": 
							(
								theCtrl = Bezier_Float()
								if theNodeDef[4][5] != undefined do theCtrl.value = theNodeDef[4][5][1]
								theMagmaHolder.setNodeProperty theNewNode "forceInteger" true
								if theNodeDef[4][17] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][17]
								)	
							)
							"Float": 
							(
								theCtrl = Bezier_Float()
								if theNodeDef[4][5] != undefined do theCtrl.value = theNodeDef[4][5][2]
								if theNodeDef[4][18] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][18]
								)	
							)
							"Vector": 
							(
								theCtrl = Point3_XYZ()
								if theNodeDef[4][5] != undefined do theCtrl.value = theNodeDef[4][5][3]
								if theNodeDef[4][19] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][19]
								)	
							)
						)
					)
					else
					(
						case theNodeDef[3][2] of
						(
							"Integer": 
							(
								theCtrl = theCtrl[1].controller
								if classof theCtrl == Float_List do theCtrl = theCtrl[1].controller
								theMagmaHolder.setNodeProperty theNewNode "forceInteger" true
								if theNodeDef[4][17] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][17]
								)									
							)
							"Float": 
							(
								theCtrl = theCtrl[2].controller
								if classof theCtrl == Float_List do theCtrl = theCtrl[1].controller
								if theNodeDef[4][18] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][18]
								)									
							)
							"Vector": 
							(
								theCtrl = theCtrl[3].controller
								if classof theCtrl == Point3_List do theCtrl = theCtrl[1].controller
								if theNodeDef[4][19] != undefined do
								(
									theMagmaHolder.declareExtensionProperty theNewNode "Internal_TrackConnection" 
									theMagmaHolder.setNodeProperty theNewNode "Internal_TrackConnection" theNodeDef[4][19]
								)									
							)
						)							
					)
					theMagmaHolder.setNodeProperty theNewNode "controller" theCtrl
					
				)
			)--end if input
			
			
			--HANDLING OPERATOR NODES
			if theNodeDef[1] == "Operator" do
			(
				local theType = theNodeDef[3][1]
				case theType of
				(
					"DNoise": theType = "VecNoise"
					"ToScalar": theType = "Breakout"
					"ToInteger": theType = "ToInt"
					"RayIntersect": theType = "IntersectRay"
					"ToQuat": theType = "VectorsToQuat"
					"FromQuat": theType = "QuatToVectors"
					"ToView": theType = "ToCamera"
					"FromView": theType = "FromCamera"
					"SurfDataValue": theType = "Elbow"
					"DotProduct": theType = "VectorDot"
					"CrossProduct": theType = "VectorCross"							
				)
				theNewNode = theMagmaHolder.createNode theType
				--format "theNewNode =% %\n" theNewNode theType
				if theNewNode == -1 do  --and theType != "SurfDataValue"
				(
					theNewNode = theMagmaHolder.createNode "Missing"
					theMagmaHolder.setNumNodeInputs theNewNode (for i in theNodeDef[2] where i != undefined and i != 0 collect i).count
					theMagmaHolder.setNumNodeOutputs theNewNode 1
				)
				if findItem #("FromCamera" , "ToCamera", "FromWorld", "ToWorld", "FromSpace", "ToSpace") theType > 0 do
				(
					--format "theNodeDef = %\n" theNodeDef
					theMagmaHolder.setNodeProperty theNewNode "inputType" theNodeDef[3][2]
					if matchPattern theType pattern:"*Space" do
					(
						local theNode = getNodeByName theNodeDef[3][3]
						if isValidNode theNode do
							theMagmaHolder.setNodeProperty theNewNode "node" theNode
					)
				)
				if theType == "VecNoise" or theType == "Noise" do
				(
					local theInput2 = try(theParrentArray[theNodeDef[2][2]][4][5][1])catch(4)
					local theInput3 = try(theParrentArray[theNodeDef[2][3]][4][5][2])catch(0.5)
					theMagmaHolder.setNodeProperty theNewNode "numOctaves" theInput2
					theMagmaHolder.setNodeProperty theNewNode "lacunarity" theInput3
				)
				if theType == "Breakout" or theType == "QuatToVectors" do
				(
					local theWire = theNodeDef[2][2]
					local theOutput = ::KrakatoaChannelEditor_NodeTreeData[theWire][4][5][1]
					theMagmaHolder.declareExtensionProperty theNewNode "Internal_OutputSelection" 
					theMagmaHolder.setNodeProperty theNewNode "Internal_OutputSelection" theOutput
				)
				if theNodeDef[3][1] == "SurfDataValue" do --if the node is SurfaceData, collect info about it
				(
					local theOutput = findItem #("Position","Valid","MeshIndex","FaceIndex","SignedDistance","FaceNormal","BaryCoords") theNodeDef[3][2] --if this is 0, need FaceQuery, otherwise it is handled by the kdtree node itself
					if theOutput == 0 then
					(
						local theFQNode =  theMagmaHolder.createNode "FaceQuery"
						theMagmaHolder.setNumNodeInputs theFQNode 4 
						theMagmaHolder.setNumNodeOutputs theFQNode 1 
						theMagmaHolder.setNodeInputDefaultValue theFQNode 2 0
						theMagmaHolder.setNodeInputDefaultValue theFQNode 3 0
						theMagmaHolder.setNodeInputDefaultValue theFQNode 4 [0.333333,0.333333,0.333333]
						theMagmaHolder.setNodeProperty theFQNode "exposePosition" false
						local theChannelToExpose = case theNodeDef[3][2] of
						(
							"FaceMatID": "MtlIndex"
							default: theNodeDef[3][2]
						)
						theMagmaHolder.setNodeProperty theFQNode "channels" theChannelToExpose
						theMagmaHolder.replaceNode theNewNode theFQNode
						--theMagmaHolder.DeclareExtensionProperty theFQNode "Position"
						--theMagmaHolder.SetNodeProperty theFQNode "Position" [710,90]
						
					)
					else
					(
						theMagmaHolder.declareExtensionProperty theNewNode "Internal_SurfDataType" 
						theMagmaHolder.setNodeProperty theNewNode "Internal_SurfDataType" theOutput						
					)
					
				)
				if theType == "Curve" do
				(
					local theInterface = theMagmaHolder.getNodeProperty theNewNode "curve" 
					theInterface.setNumPoints theNodeDef[3][2].count 
					for j = 1 to theNodeDef[3][2].count do
					(
						local theFlags = 0
						theFlags = bit.set theFlags 1 theNodeDef[3][2][j][4]
						theFlags = bit.set theFlags 2 theNodeDef[3][2][j][5]
						theInterface.setPoint j theNodeDef[3][2][j][1] theNodeDef[3][2][j][2] theNodeDef[3][2][j][3] theFlags 
					)
				)
			)
			if theNewNode != undefined and theNewNode != -1 do
			(
				theMagmaHolder.DeclareExtensionProperty theNewNode "Position" 
				theMagmaHolder.setNodeProperty theNewNode "Position" theNodeDef[4][1]

				theMagmaHolder.DeclareExtensionProperty theNewNode "Collapsed" 
				theMagmaHolder.setNodeProperty theNewNode "Collapsed" theNodeDef[4][2]

				theMagmaHolder.DeclareExtensionProperty theNewNode "Selected" 
				theMagmaHolder.setNodeProperty theNewNode "Selected" theNodeDef[4][3]
				
				if theNodeDef[4][14] == true do
				(
					theMagmaHolder.DeclareExtensionProperty theNewNode "FlipSocketPosition" 
					theMagmaHolder.setNodeProperty theNewNode "FlipSocketPosition" true
				)
				
				if theNodeDef[4][12] == true do
				(
					theMagmaHolder.DeclareExtensionProperty theNewNode "Exposed" 
					theMagmaHolder.setNodeProperty theNewNode "Exposed" theNodeDef[4][12]
					::Krakatoa_KMFRequiresExposureUpdate = true
				)
				
				if findItem #("IntersectRay", "NearestPoint") theType > 0 do
				(
					theMagmaHolder.DeclareExtensionProperty theNewNode "HideUnconnectedOutputs" 
					theMagmaHolder.setNodeProperty theNewNode "HideUnconnectedOutputs" true			
				)

				theMagmaHolder.DeclareExtensionProperty theNewNode "Name" 
				theMagmaHolder.setNodeProperty theNewNode "Name" theNodeDef[4][8]
				
				theMagmaHolder.DeclareExtensionProperty theNewNode "Notes" 
				local theNotes = theNodeDef[4][16]
				if theNotes == undefined do theNotes = ""
				theMagmaHolder.setNodeProperty theNewNode "Notes" theNotes			
			)
			theNewNode
		),
		

		
		fn convertKMFBLOP theMagmaHolder theNodesList theNodeDef theMaterialLibrary:  =
		(
			local BLOPArray = #()
			local theNewBLOP = theMagmaHolder.createNode "BLOP"
			theMagmaHolder.declareExtensionProperty theNewBLOP "Name" 
			theMagmaHolder.setNodeProperty theNewBLOP "Name" theNodeDef[3][1]
			local theInputConnectors = for i in theNodeDef[3][2] where i[1] == "Connector" collect i[3][1]
			for i = 1 to theInputConnectors.count do
			(
				theMagmaHolder.declareExtensionProperty theNewBLOP ("BLOPInputSocketName"+i as string)
				theMagmaHolder.setNodeProperty theNewBLOP ("BLOPInputSocketName"+i as string) theInputConnectors[i]
			)
			
			theMagmaHolder.SetNumNodeInputs theNewBLOP theInputConnectors.count --theNodeDef[2].count
			theMagmaHolder.SetNumNodeOutputs theNewBLOP 1
			
			theMagmaHolder.DeclareExtensionProperty theNewBLOP "Position" 
			theMagmaHolder.setNodeProperty theNewBLOP "Position" theNodeDef[4][1]
			
			if theNodeDef[4][14] == true do
			(
				theMagmaHolder.DeclareExtensionProperty theNewBLOP "FlipSocketPosition" 
				theMagmaHolder.setNodeProperty theNewBLOP "FlipSocketPosition" true
			)			
			
			theMagmaHolder.PushEditableBLOP theNewBLOP
			
			for i = 1 to theNodeDef[3][2].count do
			(
				local theNodeDef2 = theNodeDef[3][2][i]
				if theNodeDef2[1] == "BlackOp" then
					BLOPArray[i]=StokeFieldMFEditor_Functions.convertKMFBLOP theMagmaHolder BLOPArray theNodeDef2
				else
					BLOPArray[i]=StokeFieldMFEditor_Functions.convertKMFNode theMagmaHolder BLOPArray theNodeDef2 i theParrentArray:theNodeDef[3][2] theMaterialLibrary:theMaterialLibrary
			)			
			theMagmaHolder.PopEditableBLOP()
			#(theNewBLOP, BLOPArray)
		),
		
		fn connectKMFNode theMagmaHolder theNodesList theNodeDef theNodeIndex =
		(
			local theConnectionsArray = theNodeDef[2]
			
			for c = 1 to theConnectionsArray.count where theConnectionsArray[c] != undefined and theConnectionsArray[c] > 0 do 
			(
				local targetIndex = theConnectionsArray[c]
				if targetIndex  != undefined do
				(
					local connectToNode = theNodesList[targetIndex]
					if classof connectToNode == Array do connectToNode = connectToNode[1]
					if connectToNode != undefined and connectToNode != -1 and theNodesList[theNodeIndex] != undefined and theNodesList[theNodeIndex] != -1 do 
					(
						local OutputConnection = theMagmaHolder.getNodeProperty connectToNode "Internal_OutputSelection"
						if OutputConnection == undefined do OutputConnection = 1
						local OutputConnection2 = theMagmaHolder.getNodeProperty theNodesList[theNodeIndex] "Internal_SurfDataType"
						if OutputConnection2 != undefined do OutputConnection = OutputConnection2
						if findItem #("NearestPoint","IntersectRay") (theMagmaHolder.getNodeType connectToNode) > 0 and theMagmaHolder.getNodeType theNodesList[theNodeIndex] == "FaceQuery" then
						(
							theMagmaHolder.SetNodeInput theNodesList[theNodeIndex] 1 (theMagmaHolder.getNodeInput connectToNode 1)[1] 1 --set geometry input of the FaceQuery to the same node as the kdtree operator
							theMagmaHolder.SetNodeInput theNodesList[theNodeIndex] 2 connectToNode 3 --connect ObjIndex
							theMagmaHolder.SetNodeInput theNodesList[theNodeIndex] 3 connectToNode 4 --connect FaceIndex
							theMagmaHolder.SetNodeInput theNodesList[theNodeIndex] 4 connectToNode 7 --connect BaryCoords
						)
						else
						(
							theMagmaHolder.SetNodeInput theNodesList[theNodeIndex] c connectToNode OutputConnection
						)
					)
				)
			)
		),		
		
		fn connectKMFBLOP theMagmaHolder theNodesList theNodeDef =
		(
			theMagmaHolder.PushEditableBLOP theNodesList[1]
			local theConnectors = for i = 1 to theNodeDef[3][2].count where theNodeDef[3][2][i][1] == "Connector" collect i
			
			for i = 1 to theNodeDef[3][2].count do
			(
				local theNodeDef2 = theNodeDef[3][2][i]
				if theNodeDef2[1] == "BlackOp" then
				(
					StokeFieldMFEditor_Functions.connectKMFBLOP theMagmaHolder theNodesList[2] theNodeDef2 
				)
				else
					StokeFieldMFEditor_Functions.connectKMFNode theMagmaHolder theNodesList[2] theNodeDef2 i
				
				for c = 1 to theNodeDef2[2].count do
				(
					local theIndex = findItem theConnectors theNodeDef2[2][c] 
					--format "theConnectors = %  %  theIndex = %\n" theConnectors theNodeDef2[2][c] theIndex
					local theNodeToConnect = theNodesList[2][i]
					if theIndex > 0 do theMagmaHolder.SetNodeInput theNodeToConnect c (theMagmaHolder.GetNodeInput (theMagmaHolder.CurrentBLOP) theIndex)[1] theIndex	 
				)
				
				if theNodeDef2[1] == "OutputConnector" do
				(
					theMagmaHolder.SetOutput 1 theNodesList[2][theNodeDef2[2][1]] 1
				)
			)			
			theMagmaHolder.PopEditableBLOP()
		),		
		
		fn convertKCM theKCM theMagmaHolder  =
		(
			with PrintAllElements on ::KrakatoaChannelEditor_NodeTreeData = execute theKCM.internalFlow
			StokeFieldMFEditor_Functions.convertKMF theMagmaHolder
		),
		
		fn ConvertConnectedTrack theMagmaHolder theIndex =
		(
			local theOldNode = (theMagmaHolder.GetNodes())[theIndex]
			local theConnection = theMagmaHolder.GetNodeProperty theOldNode "Internal_TrackConnection"
			if theConnection != undefined do
			(
				local theFS = filterString theConnection "$.'"
				if theFS.count > 0 and matchPattern theFS[theFS.count] pattern:"controller" do deleteItem theFS theFS.count
				local theNewNode = theMagmaHolder.createNode "InputObject"
				theMagmaHolder.replaceNode theOldNode theNewNode
				
				thePropertyName = ""
				local theNode = getNodeByName theFS[1]
				if theFS.count > 3 and isValidNode (getNodeByName (theFS[1]+"."+theFS[2])) then
				(
					theNode = getNodeByName (theFS[1]+"."+theFS[2])
					for j = 3 to theFS.count-1 do thePropertyName += theFS[j] + "."
					thePropertyName += theFS[theFS.count]
				)
				else
				(
					for j = 2 to theFS.count-1 do thePropertyName += theFS[j] + "."
					thePropertyName += theFS[theFS.count]
				)
				
				theMagmaHolder.setNodeProperty theOldNode "object" theNode
				local thePropNode = theMagmaHolder.createNode "PropertyQuery"
				theMagmaHolder.setNodeProperty thePropNode "properties" thePropertyName
				local existingOutConnections = 	(StokeFieldMFEditor_Functions.getConnectedNodes theMagmaHolder theOldNode)[1]
				for j in existingOutConnections do
				(
					theMagmaHolder.setNodeInput j[1] j[2] thePropNode 1
				)
				theMagmaHolder.setNodeInput thePropNode 1 theOldNode 1
			)					
		),		
		
		fn convertKMFAllConnectedTracks theMagmaHolder =
		(
			local nodeArray = theMagmaHolder.GetNodes()
			local theCount = nodeArray.count
			for i = 1 to theCount do
			(
				StokeFieldMFEditor_Functions.ConvertConnectedTrack theMagmaHolder i
				local theID = nodeArray[i]
				if (theMagmaHolder.getNodeType theID) == "BLOP" do
				(
					theMagmaHolder.pushEditableBLOP theID
					StokeFieldMFEditor_Functions.convertKMFAllConnectedTracks theMagmaHolder
					theMagmaHolder.popEditableBLOP()
				)
			)			
		),
		
		fn convertKMF theMagmaHolder theFile:"" theMaterialLibrary:#() =
		(
			if doesFileExist theFile do
			(
				::KrakatoaChannelEditor_NodeTreeData = undefined
				fileIn theFile
				local theMatLibPath = (getFileNamePath theFile + getFileNameFile theFile + ".mat")
				--theMaterialLibrary = undefined
				if doesFileExist theMatLibPath do theMaterialLibrary = loadTempMaterialLibrary theMatLibPath
			)
			--print KrakatoaChannelEditor_NodeTreeData
			if ::KrakatoaChannelEditor_NodeTreeData != undefined do
			(
				theMagmaHolder.reset()
				theNodesList = #()
				for i = 1 to ::KrakatoaChannelEditor_NodeTreeData.count do
				(
					local theNodeDef = ::KrakatoaChannelEditor_NodeTreeData[i]
					if theNodeDef[1] == "BlackOp" then
					(
						theNodesList[i] = StokeFieldMFEditor_Functions.convertKMFBLOP theMagmaHolder theNodesList theNodeDef
					)
					else
					(
						theNodesList[i] = StokeFieldMFEditor_Functions.convertKMFNode theMagmaHolder theNodesList theNodeDef i theParrentArray:KrakatoaChannelEditor_NodeTreeData theMaterialLibrary:theMaterialLibrary 
					)
				)
				--with printAllElements on format "%\n" theNodesList
				
				--HANDLE CONNECTIONS
				for iteration = 1 to 2 do --need two passes to handle kdtree nodes and SurfDataValue reconnecting when created in opposite order
					for i = 1 to ::KrakatoaChannelEditor_NodeTreeData.count do
					(
						local theNodeDef = ::KrakatoaChannelEditor_NodeTreeData[i]
						if theNodeDef[1] == "BlackOp" then
						(
							for c = 1 to theNodeDef[2].count where theNodeDef[2][c] != undefined and theNodeDef[2][c] > 0 do
							(
								local connectToNode = theNodesList[theNodeDef[2][c]]
								local OutputConnection = theMagmaHolder.getNodeProperty connectToNode "Internal_OutputSelection"
								if OutputConnection == undefined do OutputConnection = 1
								theMagmaHolder.SetNodeInput theNodesList[i][1] c connectToNode OutputConnection
							)
							StokeFieldMFEditor_Functions.connectKMFBLOP theMagmaHolder theNodesList[i] theNodeDef 
						)
						else
						(
							StokeFieldMFEditor_Functions.connectKMFNode theMagmaHolder theNodesList theNodeDef i
						)
					)--end i loop	
				
				--HANDLE VALUE TRACK CONNECTIONS BY TURNING INPUT NODES INTO OBJECT INPUTS WITH PROPERTY QUERY
				StokeFieldMFEditor_Functions.convertKMFAllConnectedTracks theMagmaHolder 
				
			)--end if array is defined
		),
		
		fn convertOldScenesToNewMagmaFunction =
		(
			if ::Krakatoa_ConvertMagmaVersion == false do return false
			local oldGlobalHolders = #()
			for o in (getClassInstances KrakatoaGlobalDataHolder) do
			(
				for i in (refs.dependentNodes o) where classof i.baseobject == KrakatoaGlobalDataHolder do 
					append oldGlobalHolders i
			)
			for o in oldGlobalHolders  do o.baseobject = createInstance KrakatoaMXGlobalDataHolder
			local theOldMagmaModifiers = getClassInstances KrakatoaChannelsModifier
			for m in theOldMagmaModifiers do
			(
				local theNodes = refs.DependentNodes m
				--FranticParticles.LogProgress (">Converting KCM ["+m.name+"] to Magma 2...")
				local theNewMod = MagmaModifier()
				::KrakatoaChannelEditor_NodeTreeData = execute m.Flow
				::Krakatoa_KMFRequiresExposureUpdate = false
				StokeFieldMFEditor_Functions.convertKMF theNewMod.MagmaHolder theMaterialLibrary:m.TextureMapSources
				for n in theNodes do
				(
					for i = 1 to n.modifiers.count do 
					(
						oldModifier = n.modifiers[i]
						if oldModifier == m do
						(
							try(
								superclassof n
								if i == 1 then 
									addModifier n theNewMod
								else
									addModifier n theNewMod before:(i-1)
								superclassof n
								theNewMod.enabled = oldModifier.enabled
								theNewMod.name = oldModifier.name
								theNewMod.MagmaHolder.AutomaticRenameOFF = oldModifier.AutomaticRenameOFF
								deleteModifier n (i+1)
								--FranticParticles.LogProgress ("  +Replaced KCM with new Magma 2 Modifier in object ["+n.name+"]")
								if ::Krakatoa_KMFRequiresExposureUpdate == true do
								(
									local resultingRollout = StokeFieldMFEditor_Functions.OpenMagmaFlowEditor theNewMod.MagmaHolder
									resultingRollout.exposeControlsToModifier()
									destroyDialog resultingRollout
								)
							)catch()
						)--end if modifier found
					)--end i loop
				)--end n loop
			)--end m loop
			
			local rem = MaxOps.GetCurRenderElementMgr() 
			for i = (rem.numrenderelements() - 1)  to 0 by -1 do
			(
				local o = rem.getrenderelement i
				if classof o == Krakatoa_CustomData_OBSOLETE do
				(
					--FranticParticles.LogProgress (">Converting Krakatoa CustomData Render Element ["+o.elementName +"] to Magma 2...")
					local theNewRenderElement = Krakatoa_CustomData()
					::KrakatoaChannelEditor_NodeTreeData = execute o.magmaHolder.Flow
					StokeFieldMFEditor_Functions.convertKMF theNewRenderElement.MagmaHolder
					rem.RemoveRenderElement o
					rem.addRenderElement theNewRenderElement 
				)
			)--end i loop
		),
		
		fn convertOldScenesToNewMagma =
		(
			if ::Krakatoa_ConvertMagmaVersion == false do return false
			local theOldMagmaModifiers = getClassInstances KrakatoaChannelsModifier
			local theCount = theOldMagmaModifiers.count 
			local attempts = 0
			while theOldMagmaModifiers.count > 0 and attempts < 20 do
			(
				attempts += 1
				::StokeFieldMFEditor_Functions.convertOldScenesToNewMagmaFunction()
				theOldMagmaModifiers = getClassInstances KrakatoaChannelsModifier
			)
			--if theCount > 0 do FranticParticles.LogProgress ("+Converted "+theCount as string +" KCM(s) to Magma 2, "+attempts as string + " Iteration(s) Performed.")
		),
		
		fn closeAllEditors =
		(
			for i = MagmaFlowEditor_CurrentEditors.count to 1 by -1 do try(destroyDialog MagmaFlowEditor_CurrentEditors[i][2])catch()
			for i = MagmaFlowEditor_CurrentCurveEditors.count to 1 by -1 do try(destroyDialog MagmaFlowEditor_CurrentCurveEditors[i][3])catch()			
		),
		
		fn ErrorCallback theMagmaHolder nodeID errorMessage futureArgs =
		(
			local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == theMagmaHolder.magmaHolder collect i[2])
			if existingEditor.count > 0 then
			(	
				existingEditor = existingEditor[1]
				try(existingEditor.updateErrorLog NodeID:nodeID ErrorMessage:errorMessage)catch()
			)
		),
		
		fn loadPreset theFileToLoad theMagmaNode theMagmaOwner =
		(
			global MagmaNode = theMagmaNode
			--::MagmaFlowEditor_Genome_Properties = #()
			--::MagmaFlowEditor_Genome_MeshIterationMode = "Vertices"
			theMagmaNode.Loading = true
			fileIn theFileToLoad
			theMagmaNode.Loading = false
			--for p in ::MagmaFlowEditor_Genome_Properties do 
--				try(setProperty theMagmaOwner p[1] p[2])catch()

			--GenomeModifier_ImplementationObject.setIterationMode theMagmaOwner ::MagmaFlowEditor_Genome_MeshIterationMode
			 
			--MagmaFlowEditor_Rollout = StokeFieldMFEditor_Functions.OpenMagmaFlow magmaNode theMagmaOwner theImplementationObject theScript offscreen:true
			--MagmaFlowEditor_Rollout.exposeControlsToModifier()
			--destroyDialog MagmaFlowEditor_Rollout
		),

		fn createPresetsRCMenu mode:#macro =
		(
			local theUserFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\Genome\\MagmaFlows"
			local theDirectories = sort (getDirectories (theUserFolder+ "\\*"))
			theDirectories = for i in theDirectories collect 
			(
				local theFS = (filterString i "\\")
				local theDir = theFS[theFS.count]
				if matchPattern theDir pattern:"BlackOps" or matchPattern theDir pattern:"Temp" then dontcollect else (theUserFolder +"\\"+ theDir)
			)
			theFoldersList = join #(theUserFolder) theDirectories
			
			local txt = "" as stringstream
			local txt2 = "" as stringstream
			format "rcmenu Genome_Presets_RCMenu (\n" to:txt
			
			format "menuItem mnu_explorePresetsFolder \"EXPLORE Presets Folder...\"\n" to:txt
			format "separator sep_explorePresetsFolder \n" to:txt
			format "on mnu_explorePresetsFolder picked do shellLaunch \"explorer.exe\" @\"%\"\n" theUserFolder to:txt
			
			local cnt = 0
			for theDir = 1 to theFoldersList.count do
			(
				local theFilesList = sort (getFiles (theFoldersList[theDir] +"\\*.MagmaScript" ) )
				theFilesList = for i in theFilesList where not matchPattern (getFileNameFile i) pattern:"_*" collect i
				local theDirName =  (filterString theFoldersList[theDir] "\\") 
				if theDir != 1 do format "subMenu \"%\" (\n" theDirName[theDirName.count] to:txt
				for f in theFilesList do
				(
					cnt +=1
					format "menuItem mnu_% \"%\" \n" cnt (getFileNameFile f) to:txt
					if mode == #macro then
						format "on mnu_% picked do (StokeFieldMFEditor_Functions.AddModifierToSelectedObjects \"%\")\n" cnt f to:txt2
					else
					(
						format "on mnu_% picked do (\ntry(\n" cnt to:txt2
						format "local theMagmaOwner = modPanel.getCurrentObject()\n" to:txt2
						format "local theMagma = theMagmaOwner.magmaHolder\n" to:txt2
						
						format "theMagma.Reset()\n" to:txt2
						format "StokeFieldMFEditor_Functions.loadPreset \"%\" theMagma theMagmaOwner\n" f to:txt2
						format "if not theMagma.AutomaticRenameOFF do (\n" to:txt2
						format "(modPanel.getCurrentObject()).name = \"GNM_%\" \n" (getfilenamefile f) to:txt2
						format "modPanel.setCurrentObject  (modPanel.getCurrentObject())  \n" to:txt2
						format "))catch())\n" to:txt2
					)
				)
				if theDir != 1 do format ")\n" to:txt
			)
			
			format "%\n" (txt2 as string) to:txt
			format ")\n" to:txt
			global Genome_Presets_RCMenu 
			execute (txt as string)
			popupmenu ::Genome_Presets_RCMenu pos:mouse.screenpos
		),			
		fn AddModifierToSelectedObjects theFileToLoad =
		(
			max modify mode
			local theObjects = (for o in selection where findItem GeometryClass.classes (classof o) > 0 and classof o != TargetObject collect o)
			
			if theObjects.count == 1 and modPanel.getCurrentObject() != undefined then
			(
				local theMagmaModifier = GenomeModifier()
				local result = try((modPanel.addModToSelection theMagmaModifier);true)catch(false)
				if result do
				(
					theMagmaModifier.name = "GNM_"+ getFileNameFile theFileToLoad
					StokeFieldMFEditor_Functions.loadPreset theFileToLoad theMagmaModifier.magmaHolder theMagmaModifier
					theMagmaModifier.MagmaHolder.autoUpdate = true
				)
			)
			else
			(
				if keyboard.shiftPressed then
				(
					for o in theObjects do 
					(
						select o
						local theMagmaModifier = GenomeModifier()
						local result = try((modPanel.addModToSelection theMagmaModifier);true)catch(false)
						if result do
						(
							theMagmaModifier.name = "GNM_"+ getFileNameFile theFileToLoad
							StokeFieldMFEditor_Functions.loadPreset theFileToLoad theMagmaModifier.magmaHolder theMagmaModifier
							theMagmaModifier.MagmaHolder.autoUpdate = true
						)
					)
				)
				else
				(
					local theMagmaModifier = GenomeModifier()
					local result = try((addModifier theObjects theMagmaModifier);true)catch(false)
					if result do
					(
						select theObjects[1]
						theMagmaModifier.name = "GNM_"+ getFileNameFile theFileToLoad
						StokeFieldMFEditor_Functions.loadPreset theFileToLoad theMagmaModifier.magmaHolder theMagmaModifier
						theMagmaModifier.MagmaHolder.autoUpdate = true
					)
				)
				select theObjects
			)			
		),
		
		fn updateErrorSwatches =
		(
			for i in MagmaFlowEditor_CurrentEditors where i != undefined and i[2].open do 
			( 
				try(i[2].updateErrorSwatch())catch()
			)
		),		
		
		fn dummyGenomeFn =
		(
			#genome
		),
		
		fn OnGenomeParamChange genomeMod paramName =
		(
			if paramName as name == #autoupdate do
			(
				for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == genomeMod.MagmaHolder do
					try(i[2].chk_autoUpdateModifier.state = genomeMod.MagmaHolder.autoUpdate)catch()
			)
		),
		
		fn OpenMagmaFlow theMagmaHolder theMagmaOwner theImplementationObject theScript offscreen:false = --this function is called by the Implementation Object to either create the dialog or set it in focus.
		(
			local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == theMagmaHolder collect i)
			if existingEditor.count == 0 then
			(
				local result = fileIn theScript
				
				local done = false
				for i = 1 to ::MagmaFlowEditor_CurrentEditors.count+1 where MagmaFlowEditor_CurrentEditors[i] == undefined and not done do
				(
					done = true
					::MagmaFlowEditor_CurrentEditors[i] = #(theMagmaHolder, result[1], -1, result[3], result[4], theMagmaOwner, theImplementationObject, ::StokeFieldMFEditor_Functions, &Stoke_MagmaFlowColors )
				)
				local theIniFileLocation = GetDir #plugcfg
				theIniFileLocation +=  "\\"+theImplementationObject.getProductTitle() +"\\" 
				
				local theSize = StokeFieldMFEditor_Functions.getEditorProperty theMagmaHolder "DialogSize"
				theSize = if theSize != undefined then
					execute theSize
				else
					execute (getIniSetting (theIniFileLocation + "MagmaFlowEditor_Preferences.ini") "Editor" "Size")
				if theSize == OK do theSize = [980,600]
				if theSize.x < 400 do theSize.x = 400
				if theSize.y < 200 do theSize.y = 200

				local thePos = StokeFieldMFEditor_Functions.getEditorProperty theMagmaHolder "DialogPosition"
				thePos = if thePos != undefined then
					execute thePos 
				else
					execute (getIniSetting (theIniFileLocation + "MagmaFlowEditor_Preferences.ini") "Editor" "Position")
				
				local theScreens = (dotNetClass "System.Windows.Forms.Screen").AllScreens
				local minX = 100000
				local minY = 100000
				local maxX = -100000
				local maxY = -100000
				for aScreen in theScreens do 
				(
					local theBounds = aScreen.Bounds
					if theBounds.Left < minX do minX = theBounds.Left
					if theBounds.Right > maxX do maxX = theBounds.Right
					if theBounds.Top < minY do minY = theBounds.Top
					if theBounds.Right > maxY do maxY = theBounds.Right
					--format "L:% R:% T:% B:% W:% H:%\n" theBounds.Left theBounds.Right theBounds.Top theBounds.Bottom theBounds.Width theBounds.Height
				)				
				
				if thePos == OK do thePos = [100,100]
				if thePos.x < minX do thePos.x = minX
				if thePos.y < minY do thePos.y = minY
				if thePos.x > maxX - theSize.x do thePos.x = maxX - theSize.x
				if thePos.y > maxY - theSize.y do thePos.y = maxY - theSize.y
				
				if offscreen == true do thePos.x = -100000
				createDialog result[1] theSize.x theSize.y thePos.x thePos.y style:#(#style_titlebar, #style_border, #style_sysmenu, #style_resizing, #style_minimizebox, #style_maximizebox) menu:result[2]
				if offscreen == true do try(result[1].visible = false)catch()
				
				result[1]
			)
			else
			(
				try(if existingEditor[1][2].placement == #minimized do existingEditor[1][2].placement = #normal)catch()
				setFocus existingEditor[1][2]
				existingEditor[1][2]
			)
		)
	)
	
	callbacks.removeScripts id:#StokeFieldMagmaFlowEditor
	callbacks.addScript #filePreOpen "::StokeFieldMFEditor_Functions.closeAllEditors()" id:#StokeFieldMagmaFlowEditor
	callbacks.addScript #systemPostNew "::StokeFieldMFEditor_Functions.closeAllEditors()" id:#StokeFieldMagmaFlowEditor
	callbacks.addScript #systemPostReset "::StokeFieldMFEditor_Functions.closeAllEditors()" id:#StokeFieldMagmaFlowEditor
	callbacks.addScript #modPanelObjPostChange "::StokeFieldMFEditor_Functions.updateErrorSwatches()" id:#StokeFieldMagmaFlowEditor
	
	--#PRODUCT: This callback should not be called in Stoke or Genome.
	--callbacks.addScript #filePostOpen "::StokeFieldMFEditor_Functions.convertOldScenesToNewMagma()" id:#StokeFieldMagmaFlowEditor

	Ok
)