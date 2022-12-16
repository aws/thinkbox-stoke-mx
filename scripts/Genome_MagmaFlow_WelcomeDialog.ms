-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
global GenomeMagma_Welcome_Dialog 
try(destroyDialog GenomeMagma_Welcome_Dialog )catch()
rollout GenomeMagma_Welcome_Dialog  "Welcome To The GENOME MagmaFlow Editor!"
(
	label lbl_line10 "You are editing a NEW modifier which has NO NODES yet. You can either LOAD an existing flow or start CREATING nodes."
	label lbl_line20 ""
	group "LOADING A PRESET:"
	(
		label lbl_line30 "To LOAD a saved flow, look at the 'Preset Flows Explorer' rollout in the right panel of the Editor."
		label lbl_line40 "Select a FOLDER from the top list and DOUBLE-CLICK a preset name in the bottom list to LOAD it."
	)
	label lbl_line50 ""
	
	group "CREATING A FLOW:"
	(
		label lbl_line100 "To start CREATING nodes, RIGHT-CLICK anywhere on the grid to open the context menu."
		label lbl_line110 "A MagmaFlow must define at least one OUTPUT channel - select 'Create Output Node' or press [CTRL]+[O]."
		label lbl_line120 "In the Output node, specify the channel you want to set."
		label lbl_line130 ""
		label lbl_line140 "To access ALL available nodes, select 'Create Nodes Menu' or press the [`/~] key."
		label lbl_line150 "Alternatively, click on the DEPOT Categories and DRAG nodes from it into the Editor."
		label lbl_line160 "You can also drag a connection from an unconnected socket of a node to open a nodes menu."
		label lbl_line170 ""
		label lbl_line180 "If you select an existing node and create a new node, MagmaFlow will attempt to CONNECT AUTOMATICALLY."
		button btn_openHelp "CLICK HERE to open the GENOME Online Help with more information about creating flows." border:false
	)
	
	button btn_close "CLOSE THIS MESSAGE TO CONTINUE..." height:30 
	
	checkbox chk_dontShowThisAgain "Please do not show this message again in the future, I know what I am doing!" align:#center
	
	
	on chk_dontShowThisAgain changed state do 
	(
		setIniSetting (GetDir #plugcfg + "\\Genome\\MagmaFlowEditor_Preferences.ini") "Editor" "DoNotShowWelcomeDialog" (state as string)
	)
	on btn_close pressed do destroyDialog GenomeMagma_Welcome_Dialog 
	
)
createDialog GenomeMagma_Welcome_Dialog  600 400 modal:true