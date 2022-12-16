-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript CreateStokeField category:"Stoke" buttontext:"StokeField" tooltip:"Create Field MAGMA - hold SHIFT to set Grid To Selection Bounding Box, or create Default Grid if nothing is selected." icon:#("Stoke",5)
(
	
	if keyboard.shiftpressed then
	(
		local theBBoxMax = [50,50,50]
		local theBBoxMin = [-50,-50,-50]
		
		if selection.count > 0 then
		(
			theBBoxMin = [1,1,1]*(10^6)
			theBBoxMax = [-1,-1,-1]*(10^6)
			for o in selection do
			(
				if o.min.x < theBBoxMin.x do theBBoxMin.x = o.min.x
				if o.min.y < theBBoxMin.y do theBBoxMin.y = o.min.y
				if o.min.z < theBBoxMin.z do theBBoxMin.z = o.min.z
				if o.max.x > theBBoxMax.x do theBBoxMax.x = o.max.x
				if o.max.y > theBBoxMax.y do theBBoxMax.y = o.max.y
				if o.max.z > theBBoxMax.z do theBBoxMax.z = o.max.z
			)
		)
		local theBBoxSize = theBBoxMax-theBBoxMin
		local theObj = Stoke_Field pos:[0,0,0] wirecolor:(color 87 225 198)	 
		theObj.renderable = false
		theObj.delegate.GridSpacing = theObj.gridSpacing = (length theBBoxSize)/52.0
		
		theObj.BoundsminX=theBBoxMin.x - theObj.gridSpacing
		theObj.BoundsminY=theBBoxMin.y - theObj.gridSpacing
		theObj.BoundsminZ=theBBoxMin.z - theObj.gridSpacing
		theObj.BoundsmaxX=theBBoxMax.x + theObj.gridSpacing
		theObj.BoundsmaxY=theBBoxMax.y + theObj.gridSpacing
		theObj.BoundsmaxZ=theBBoxMax.z + theObj.gridSpacing
		select theObj
		max modify mode
		manipulateMode=true
		--createDefaultVelocity theObj.delegate.MagmaHolder
	)
	else
	(
		tool StokeFieldCreator
		(
			local theObj, startpoint, endpoint
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						--in coordsys grid 
						theObj=Stoke_Field pos:gridPoint wirecolor:(color 87 225 198)	 
						theObj.BoundsMinX = gridPoint.x
						theObj.BoundsMinY = gridPoint.y
						theObj.BoundsMinZ = gridPoint.z	
						select theObj
						pushprompt "HOLD DOWN Left Mouse Button and MOVE the Mouse to DEFINE Grid Base Extents. RELEASE LMB to DEFINE the Grid Height..."
					)
					2:
					(
						endpoint = gridPoint
						popprompt()
						pushprompt "LEFT-CLICK or RIGHT-CLICK to Set the HEIGHT and FINISH Grid creation."
					)
					3: (
						popprompt()
						select theObj
						max modify mode
						#stop
					)
				)
			)
			on mouseMove clickno do
			(
				if theObj != undefined do
				(
					case clickno of
					(
						2:
						(
							if gridPoint.x >= theObj.BoundsMinX+theObj.GridSpacing*2 then 
							(
								theObj.BoundsMaxX = gridPoint.x 
								theObj.BoundsMinX = startPoint.x
							)
							else 
							(
								theObj.BoundsMinX = gridPoint.x 
								theObj.BoundsMaxX = startPoint.x
							)
							if gridPoint.y >= theObj.BoundsMinY+theObj.GridSpacing*2 then 
							(
								theObj.BoundsMaxY = gridPoint.Y 
								theObj.BoundsMinY = startPoint.y
							)
							else 
							(
								theObj.BoundsMinY = gridPoint.y
								theObj.BoundsMaxY = startPoint.y
							)
							theObj.BoundsMaxZ = gridPoint.z+theObj.GridSpacing*2
							local theBox = [theObj.boundsMinX,theObj.boundsMinY,theObj.boundsMinZ] + (([theObj.boundsMaxX,theObj.boundsMaxY,theObj.boundsMaxZ]-[theObj.boundsMinX,theObj.boundsMinY,theObj.boundsMinZ])/2)
							theObj.pos = [theBox.x,theBox.y, theObj.boundsMinZ]
							local theBBoxSize = [theObj.BoundsmaxX,theObj.BoundsmaxY,theObj.BoundsmaxZ] - [theObj.BoundsminX,theObj.BoundsminY, theObj.BoundsminZ]
							theObj.GridSpacing = (length theBBoxSize)/50.0								
						)
						3:
						(
							theObj.BoundsMaxZ = amax #(0.5*length (endpoint-gridPoint), gridPoint.z+theObj.GridSpacing*2)
							local theBBoxSize = [theObj.BoundsmaxX,theObj.BoundsmaxY,theObj.BoundsmaxZ] - [theObj.BoundsminX,theObj.BoundsminY, theObj.BoundsminZ]
							theObj.GridSpacing = (length theBBoxSize)/50.0									
						)
					)
				)
			)		
		)--end tool	
		max modify mode
		manipulateMode=true
		--StartObjectCreation Stoke_Field
		startTool StokeFieldCreator		
	)
)

macroScript CreateStokeFieldSim category:"Stoke" buttontext:"Field Sim" tooltip:"Create Stoke Field Simulator - hold SHIFT to set Grid To Selection Bounding Box, or create Default Grid if nothing is selected." icon:#("Stoke",4)
(

	fn createDefaultDensity magmaNode = 
	(
		node0 = magmaNode.createNode "Output" 
		magmaNode.setNumNodeInputs node0 1 
		magmaNode.setNumNodeOutputs node0 0 
		magmaNode.setNodeProperty node0 "channelName" "Density"
		magmaNode.setNodeProperty node0 "channelType" "float32"
		magmaNode.DeclareExtensionProperty node0 "Position"
		magmaNode.SetNodeProperty node0 "Position" [644,212]
		--------------------------------------------
		node1 = magmaNode.createNode "InputValue" 
		magmaNode.setNumNodeInputs node1 0 
		magmaNode.setNumNodeOutputs node1 1 
		magmaNode.setNodeProperty node1 "forceInteger" false
		ctrl=bezier_float(); ctrl.value = 0.0
		magmaNode.setNodeProperty node1 "controller" ctrl
		magmaNode.DeclareExtensionProperty node1 "Position"
		magmaNode.SetNodeProperty node1 "Position" [518,0]
		--------------------------------------------
		magmaNode.setNodeInput node0 1 node1 1
	)
	
	local theBBoxMax = [50,50,50]
	local theBBoxMin = [-50,-50,-50]
	
	if keyboard.shiftpressed then
	(
		if selection.count > 0 then
		(
			theBBoxMin = [1,1,1]*(10^6)
			theBBoxMax = [-1,-1,-1]*(10^6)
			for o in selection do
			(
				if o.min.x < theBBoxMin.x do theBBoxMin.x = o.min.x
				if o.min.y < theBBoxMin.y do theBBoxMin.y = o.min.y
				if o.min.z < theBBoxMin.z do theBBoxMin.z = o.min.z
				if o.max.x > theBBoxMax.x do theBBoxMax.x = o.max.x
				if o.max.y > theBBoxMax.y do theBBoxMax.y = o.max.y
				if o.max.z > theBBoxMax.z do theBBoxMax.z = o.max.z
			)
		)
		local theBBoxSize = theBBoxMax-theBBoxMin
		local theObj = StokeFieldSim pos:[0,0,0] BoundsminX:theBBoxMin.x BoundsminY:theBBoxMin.y BoundsminZ:theBBoxMin.z BoundsmaxX:theBBoxMax.x BoundsmaxY:theBBoxMax.y BoundsmaxZ:theBBoxMax.z wirecolor:(red*0.5)   
		theObj.renderable = false
		theObj.GridSpacing = (length theBBoxSize)/52.0
		--createDefaultDensity theObj.InitMagmaHolder
		manipulateMode=true
		select theObj
		max modify mode
	)
	else
	(
		tool StokeFieldSimCreator
		(
			local theObj, startpoint, endpoint
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						--in coordsys grid 
						theObj=StokeFieldSim pos:gridPoint wirecolor:(red*0.5) 
						theObj.BoundsMinX = gridPoint.x
						theObj.BoundsMinY = gridPoint.y
						theObj.BoundsMinZ = gridPoint.z	
						select theObj
						pushprompt "HOLD DOWN Left Mouse Button and MOVE the Mouse to DEFINE Grid Base Extents. RELEASE LMB to DEFINE the Grid Height..."
					)
					2:
					(
						endpoint = gridPoint
						popprompt()
						pushprompt "LEFT-CLICK or RIGHT-CLICK to Set the HEIGHT and FINISH Grid creation."
					)
					3: (
						popprompt()
						select theObj
						max modify mode
						#stop
					)
				)
			)
			on mouseMove clickno do
			(
				if theObj != undefined do
				(
					case clickno of
					(
						2:
						(
							if gridPoint.x >= theObj.BoundsMinX+theObj.GridSpacing*2 then 
							(
								theObj.BoundsMaxX = gridPoint.x 
								theObj.BoundsMinX = startPoint.x
							)
							else 
							(
								theObj.BoundsMinX = gridPoint.x 
								theObj.BoundsMaxX = startPoint.x
							)
							if gridPoint.y >= theObj.BoundsMinY+theObj.GridSpacing*2 then 
							(
								theObj.BoundsMaxY = gridPoint.Y 
								theObj.BoundsMinY = startPoint.y
							)
							else 
							(
								theObj.BoundsMinY = gridPoint.y
								theObj.BoundsMaxY = startPoint.y
							)
							theObj.BoundsMaxZ = gridPoint.z+theObj.GridSpacing*2
							local theBox = [theObj.boundsMinX,theObj.boundsMinY,theObj.boundsMinZ] + (([theObj.boundsMaxX,theObj.boundsMaxY,theObj.boundsMaxZ]-[theObj.boundsMinX,theObj.boundsMinY,theObj.boundsMinZ])/2)
							theObj.pos = [theBox.x,theBox.y, theObj.boundsMinZ]
							local theBBoxSize = [theObj.BoundsmaxX,theObj.BoundsmaxY,theObj.BoundsmaxZ] - [theObj.BoundsminX,theObj.BoundsminY, theObj.BoundsminZ]
							theObj.GridSpacing = (length theBBoxSize)/50.0								
						)
						3:
						(
							theObj.BoundsMaxZ = amax #(0.5*length (endpoint-gridPoint), gridPoint.z+theObj.GridSpacing*2)
							local theBBoxSize = [theObj.BoundsmaxX,theObj.BoundsmaxY,theObj.BoundsmaxZ] - [theObj.BoundsminX,theObj.BoundsminY, theObj.BoundsminZ]
							theObj.GridSpacing = (length theBBoxSize)/50.0									
						)
					)
				)
			)
		)
		
		max modify mode
		manipulateMode=true
		pushprompt "Click to define the Grid center..."
		startTool StokeFieldSimCreator
		--StartObjectCreation StokeFieldSim
	)
)

macroScript CreateStokeFieldForce category:"Stoke" buttontext:"Field Force" tooltip:"Create Stoke Field Force - hold SHIFT to create at origin. If a Stoke Field Sim is selected or if there is only one in the scene, it will be connected automatically." icon:#("Stoke",7)
(
	local theSim = undefined
	local theSims = for o in selection where StokeGlobalInterface.GetVelocityType o == #field collect o
	if theSims.count > 0 do theSim = theSims[1]
	
	if keyboard.shiftpressed then
	(
		local theObj = Field_Force pos:[0,0,0] iconSize:50.0
		if theSim != undefined do theObj.FieldNode = theSim
		select theObj
		max modify mode
	)
	else
	(
		tool StokeFieldForceCreator
		(
			local theObj, startpoint 
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						in coordsys grid theObj=Field_Force pos:gridPoint wirecolor:(green*0.5) 
						select theObj
					)
					3: (
						if theSim != undefined do theObj.FieldNode  = theSim
						max modify mode
						select theObj
						#stop
					)
				)
			)
			on mouseMove clickno do
			(
				if theObj != undefined do
				(
					case clickno of
					(
						2: theObj.pos = gridPoint
						3:(
							theObj.IconSize=(length (worldPoint-startpoint))
							)
					)
				)
			)
		)
		startTool StokeFieldForceCreator
	)
)

macroScript CreateStokeFieldLoader category:"Stoke" buttontext:"Field Loader" tooltip:"Create Stoke Field Loader - Hold SHIFT to create at the world origin." icon:#("Stoke",3)
(
	if keyboard.shiftpressed then
	(
		theObj=StokeFieldLoader wirecolor:(yellow*0.75)
		theObj.renderable = false
		select theObj
		max modify mode		
	)
	else
	(	
		tool StokeFieldLoaderCreator
		(
			local theObj, startpoint 
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						in coordsys grid theObj=StokeFieldLoader pos:gridPoint wirecolor:(yellow*0.75)
						theObj.renderable = false
						theObj.delegate.InWorldSpace = false
						select theObj
						max modify mode
						#stop
					)
				)
			)
		)
		startTool StokeFieldLoaderCreator
	)
)

macroScript CreateStokePRTField category:"Stoke" buttontext:"PRT Field" tooltip:"Create Stoke PRT Field object - Hold SHIFT to create at the world origin." icon:#("Stoke",6)
(
	local theSim = undefined
	local theSims = for o in selection where findItem #(#field) (StokeGlobalInterface.GetVelocityType o) > 0 collect o
	if theSims.count > 0 do theSim = theSims[1]
	if keyboard.shiftpressed then
	(
		theObj=PRT_Field wirecolor:(color 50 150 255)
		if theSim != undefined do theObj.TargetNode = theSim
		select theObj
		max modify mode
	)
	else
	(	
		tool StokePRTFieldLoaderCreator
		(
			local theObj, startpoint 
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						in coordsys grid theObj=PRT_Field pos:gridPoint wirecolor:(color 50 150 255)
						if theSim != undefined do theObj.TargetNode = theSim
						select theObj
						max modify mode
						#stop
					)
				)
			)
		)
		startTool StokePRTFieldLoaderCreator
	)
)


macroScript AddStokeFieldDisplayModifier category:"Stoke" buttontext:"Add Stoke FIELD DISPLAY Modifier" --icon:#("StokeField",1)
(
	on isEnabled return (for o in selection where validModifier o Stoke_Field_Display_Modifier collect o).count > 0
	
	on execute do
	(
		max modify mode
		local theSelection = selection as array
		if theSelection.count == 1 and ValidModifier selection[1] Stoke_Field_Display_Modifier then
		(
			try(modPanel.addModToSelection (Stoke_Field_Display_Modifier()))catch()
		)
		else
		(
			for o in theSelection where validModifier o Stoke_Field_Display_Modifier do
			(
				addModifier o (Stoke_Field_Display_Modifier())
			)
		)
	)
)

macroScript AddStokeFieldFluid category:"Stoke" buttontext:"Add Stoke FIELD FLUID MOTION Modifier" --icon:#("StokeField",1)
(
	on isEnabled return (for o in selection where validModifier o Stoke_Field_DivergenceFree collect o).count > 0
	
	on execute do
	(
		max modify mode
		local theSelection = selection as array
		if theSelection.count == 1 and ValidModifier selection[1] Stoke_Field_DivergenceFree then
		(
			try(modPanel.addModToSelection (Stoke_Field_DivergenceFree()))catch()
		)
		else
		(
			for o in theSelection where validModifier o Stoke_Field_DivergenceFree do
			(
				addModifier o (Stoke_Field_DivergenceFree())
			)
		)
	)
)

macroScript AddStokeFieldFromMesh category:"Stoke" buttontext:"Add Stoke FIELD FROM MESH Modifier" --icon:#("StokeField",1)
(
	on isEnabled return (for o in selection where validModifier o Stoke_Field_From_Mesh_Converter collect o).count > 0

	on execute do
	(
		max modify mode
		local theSelection = selection as array
		if theSelection.count == 1 and ValidModifier selection[1] Stoke_Field_From_Mesh_Converter then
		(
			try(modPanel.addModToSelection (Stoke_Field_From_Mesh_Converter()))catch()
		)
		else
		(
			for o in theSelection where validModifier o Stoke_Field_From_Mesh_Converter do
			(
				addModifier o (Stoke_Field_From_Mesh_Converter())
			)
		)
	)
)

macroScript OpenStokeFieldDataViewer category:"Stoke" buttontext:"FDV" tooltip:"Open Stoke Field Data Viewer" icon:#("Stoke",8)
(
	try(fileIn (StokeGlobalInterface.HomeDirectory+"\\Scripts\\Stoke_FieldDataViewer.ms"))catch()
)


macroScript CreateStokeFieldMesher category:"Stoke" buttontext:"Field Mesher" tooltip:"Create Stoke Field Mesher - hold SHIFT to create at origin. If a Stoke Field is selected or if there is only one in the scene, it will be connected automatically." icon:#("Stoke",12)
(
	local theSim = undefined
	local theSims = for o in selection where StokeGlobalInterface.GetVelocityType o == #field collect o
	if theSims.count > 0 do theSim = theSims[1]
		
	fn setChannel theObj theSim setIcon:true =
	(
		local theInterface = try(StokeGlobalInterface.CreateMXSField theSim)catch(undefined)
		if theInterface != undefined do 
		(
			for c in theInterface.Channels do
			(
				local theFS = filterString c "[] "
				if matchPattern theFS[2] pattern:"float*" and theFS[3]=="1" do 
				(
					theObj.SurfaceChannel = theFS[1]
					exit
				)
			)
			if setIcon == true do theObj.iconSize = amax #(length(theInterface.BoundsMax-theInterface.BoundsMin)*0.5, 50.0)
			theInterface.Clear()
		)		
	)
	
	if keyboard.shiftpressed then
	(
		local theObj = FieldMesher pos:[0,0,0] iconSize:50.0 wirecolor:(color 64 192 255) 
		if theSim != undefined do 
		(
			theObj.TargetNode = theSim
			setChannel theObj theSim setIcon:true
		)
		select theObj
		max modify mode
	)
	else
	(
		tool StokeFieldMesherCreator
		(
			local theObj, startpoint 
			on mousePoint clickno do
			(
				case clickno of
				(
					1:
					(
						startpoint = gridPoint
						in coordsys grid theObj=FieldMesher pos:gridPoint wirecolor:(color 64 192 255) iconSize:10.0
						select theObj
					)
					3: (
						if theSim != undefined do 
						(
							theObj.TargetNode  = theSim
							setChannel theObj theSim setIcon:false
						)
						max modify mode
						select theObj
						#stop
					)
				)
			)
			on mouseMove clickno do
			(
				if theObj != undefined do
				(
					case clickno of
					(
						2: theObj.pos = gridPoint
						3:(
							theObj.IconSize=(length (worldPoint-startpoint))*2
							)
					)
				)
			)
		)
		startTool StokeFieldMesherCreator
	)
)
