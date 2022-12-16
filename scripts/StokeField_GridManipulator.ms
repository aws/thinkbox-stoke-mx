-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
plugin simpleManipulator StokeFieldSimGridManip
    name:"StokeFieldSimGridManip"
    invisible:true
(

    local greenColor =  colorMan.getColor #manipulatorsActive
    local redColor =    colorMan.getColor #manipulatorsSelected
	
    on canManipulate target return (classof target == StokeFieldSim or classof target == Stoke_Field) 

    on updateGizmos do
    (
        this.clearGizmos()
		local theNode = (refs.dependentNodes target)[1]
		local theTM = inverse theNode.objectTransform
		local size = length ([target.BoundsMaxX,target.BoundsMaxY,target.BoundsMaxZ] - [target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ])/100
		local minsize = try(target.minManipulatorSize)catch(0.1)
		local maxsize = try(target.maxManipulatorSize)catch(10.0)
		if size < minsize do size = minsize
		if size > maxsize do size = maxsize
		local bbox = ([target.BoundsMaxX,target.BoundsMaxY,target.BoundsMaxZ] + [target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ])/2.0
		
		giz1 = manip.MakeSphere ([target.BoundsMinx,bbox.y,bbox.z]*theTM) size 8
		giz2 = manip.MakeSphere ([target.BoundsMaxx,bbox.y,bbox.z]*theTM) size 8
		giz3 = manip.MakeSphere ([bbox.x,target.BoundsMiny,bbox.z]*theTM) size 8
		giz4 = manip.MakeSphere ([bbox.x,target.BoundsMaxy,bbox.z]*theTM) size 8
		giz5 = manip.MakeSphere ([bbox.x,bbox.y,target.BoundsMinz]*theTM) size 8
		giz6 = manip.MakeSphere ([bbox.x,bbox.y,target.BoundsMaxz]*theTM) size 8
		
		
		gizShape = manip.makeGizmoShape()
        gizShape.addPoint ([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMiny,target.BoundsMaxz]*theTM) 	
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMaxy,target.BoundsMaxz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMaxy,target.BoundsMinz] 	*theTM)
        gizShape.addPoint ([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMiny,target.BoundsMinz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMiny,target.BoundsMaxz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxX,target.BoundsMaxY,target.BoundsMaxZ]*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMaxy,target.BoundsMinz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMiny,target.BoundsMinz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMiny,target.BoundsMaxz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMiny,target.BoundsMaxz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMaxy,target.BoundsMaxz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMaxy,target.BoundsMinz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxx,target.BoundsMaxy,target.BoundsMinz] 	*theTM)
	    gizShape.addPoint ([target.BoundsMaxX,target.BoundsMaxY,target.BoundsMaxZ]	*theTM)
	    gizShape.addPoint ([target.BoundsMinx,target.BoundsMaxy,target.BoundsMaxz] 	*theTM)
		
        this.addGizmoMesh giz1 0 greenColor redColor  
        this.addGizmoMesh giz2 0 greenColor redColor 
        this.addGizmoMesh giz3 0 greenColor redColor  
        this.addGizmoMesh giz4 0 greenColor redColor 
        this.addGizmoMesh giz5 0 greenColor redColor  
        this.addGizmoMesh giz6 0 greenColor redColor 

        this.addGizmoShape gizShape gizmoDontHitTest greenColor greenColor
		
		gizShape = manip.makeGizmoShape()
        gizShape.addPoint ([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]*theTM)
		local theSpacing = target.gridspacing
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,0,0]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,0,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,1,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[0,1,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[0,0,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,0,1]*theSpacing)*theTM)
		this.addGizmoShape gizShape gizmoDontHitTest greenColor greenColor
		
		gizShape = manip.makeGizmoShape()
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,0,0]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,1,0]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,1,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[0,1,1]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[0,1,0]*theSpacing)*theTM)
        gizShape.addPoint (([target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ]+[1,1,0]*theSpacing)*theTM)
		
		this.addGizmoShape gizShape gizmoDontHitTest greenColor greenColor


        return node.name + " Min:" + [target.BoundsMinX,target.BoundsMinY,target.BoundsMinZ] as string  + " Max:" + [target.BoundsMaxX,target.BoundsMaxY,target.BoundsMaxZ] as string 
    )

    on mouseMove m which do
    (
		if which == 0 or which == 1 do
		(
			local plane1 = manip.makePlaneFromNormal y_axis [0, 0, 0]
			local plane2 = manip.makePlaneFromNormal z_axis [0, 0, 0]
		)

		if which == 2 or which == 3 do
		(
			local plane1 = manip.makePlaneFromNormal x_axis [0, 0, 0]
			local plane2 = manip.makePlaneFromNormal z_axis [0, 0, 0]
		)
		
		if which == 4 or which == 5 do
		(
			local plane1 = manip.makePlaneFromNormal x_axis [0, 0, 0]
			local plane2 = manip.makePlaneFromNormal y_axis [0, 0, 0]
		)
		local projectedPoint = [0,0,0]
		local viewRay = this.getLocalViewRay m
		local pl = plane1.mostOrthogonal viewRay plane2
		local res = pl.intersect viewRay &projectedPoint
		local theNode = (refs.dependentNodes target)[1]
		local theTM = theNode.objectTransform
		local theBox = ([target.BoundsMaxx, target.BoundsMaxy,target.BoundsMaxz] - [target.BoundsMinx,target.BoundsMiny,target.BoundsMinz])/2
		local theCenter = theNode.pos
		projectedPoint *= theTM
		if res do 
		(
			case which of
			(
				default: ()
				0: (
					if projectedPoint.x < target.BoundsMaxx-target.GridSpacing*2 do
					(
						target.BoundsMinx = projectedPoint.x
						if keyboard.shiftPressed then 
							if keyboard.controlPressed then 
							(
								local newValue = theCenter.x-target.BoundsMinx 
								target.BoundsMaxx = theCenter.x+newValue
								target.BoundsMaxy = theCenter.y+newValue
								target.BoundsMaxz = theCenter.z+newValue
								target.BoundsMiny = theCenter.y - newValue
								target.BoundsMinz = theCenter.z - newValue
							)
							else 
								target.BoundsMaxx = theCenter.x + (theCenter.x-target.BoundsMinx)
					)
				)
				1: (
					if projectedPoint.x > target.BoundsMinx+target.GridSpacing*2 do
					(					
						target.BoundsMaxx = projectedPoint.x
						if keyboard.shiftPressed then
							if keyboard.controlPressed then 
							(
								local newValue = target.BoundsMaxx - theCenter.x
								target.BoundsMaxy = theCenter.y+newValue
								target.BoundsMaxz = theCenter.z+newValue
								target.BoundsMinx = theCenter.x - newValue
								target.BoundsMiny = theCenter.y - newValue
								target.BoundsMinz = theCenter.z - newValue
							)						
							else
								target.BoundsMinx = theCenter.x - (target.BoundsMaxx-theCenter.x)
					)
				)
				2: (
					if projectedPoint.y < target.BoundsMaxy-target.GridSpacing*2 do
					(					
						target.BoundsMiny = projectedPoint.y
						if keyboard.shiftPressed then
							if keyboard.controlPressed then 
							(
								local newValue = theCenter.y-target.BoundsMiny  
								target.BoundsMaxx = theCenter.x+newValue
								target.BoundsMaxy = theCenter.y+newValue
								target.BoundsMaxz = theCenter.z+newValue
								target.BoundsMinx = theCenter.x - newValue
								target.BoundsMinz = theCenter.z - newValue
							)
							else
								target.BoundsMaxy = theCenter.y + (theCenter.y-target.BoundsMiny)
					)
				)
				3: (
					if projectedPoint.y > target.BoundsMiny+target.GridSpacing*2 do
					(					
						target.BoundsMaxy = projectedPoint.y
						if keyboard.shiftPressed then 
							if keyboard.controlPressed then 						
							(
								local newValue = target.BoundsMaxy - theCenter.y
								target.BoundsMaxx = theCenter.x+newValue
								target.BoundsMaxz = theCenter.z+newValue
								target.BoundsMinx = theCenter.x - newValue
								target.BoundsMiny = theCenter.y - newValue
								target.BoundsMinz = theCenter.z - newValue
							)
							else
								target.BoundsMiny = theCenter.y - (target.BoundsMaxy-theCenter.y)
					)
				)
				4: (
					if projectedPoint.z < target.BoundsMaxz-target.GridSpacing*2 do
					(						
						target.BoundsMinz = projectedPoint.z 
						if keyboard.shiftPressed then 
							if keyboard.controlPressed then 						
							(
								local newValue = theCenter.z-target.BoundsMinz
								target.BoundsMaxx = theCenter.x+newValue
								target.BoundsMaxy = theCenter.y+newValue
								target.BoundsMaxz = theCenter.z+newValue
								target.BoundsMinx = theCenter.x - newValue
								target.BoundsMiny = theCenter.y - newValue
							)
							else
								target.BoundsMaxz = theCenter.z + (theCenter.z-target.BoundsMinz)
					)
				)	
				5: (
					if projectedPoint.z > target.BoundsMinz+target.GridSpacing*2 do
					(						
						target.BoundsMaxz = projectedPoint.z 
						if keyboard.shiftPressed then 
							if keyboard.controlPressed then 						
							(	
								local newValue = target.BoundsMaxz - theCenter.z
								target.BoundsMaxx = theCenter.x+newValue
								target.BoundsMaxy = theCenter.y+newValue
								target.BoundsMinx = theCenter.x - newValue
								target.BoundsMiny = theCenter.y - newValue
								target.BoundsMinz = theCenter.z - newValue
							)
							else
								target.BoundsMinz = theCenter.z - (target.BoundsMaxz-theCenter.z)
					)
				)
			)--end case
		)--end if res
    )
)
