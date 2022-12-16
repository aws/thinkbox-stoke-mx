-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript AddGenomePreset category:"Genome" tooltip:"Add a Preset Genome Modifier to the selection. Hold SHIFT to add a unique copy to multiple selected objects." icon:#("Genome",2)
(
	::GenomeMFEditor_Functions.createPresetsRCMenu mode:#macro
)