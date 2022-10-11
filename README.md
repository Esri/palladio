# ![](doc/img/Palladio_icon_v1_32.png) Palladio - CityEngine Plugin for Houdini

Palladio is a plugin for [SideFX Houdini](https://www.sidefx.com). It provides operator nodes which enable the execution of [CityEngine](https://www.esri.com/software/cityengine) ‘rules’ within Houdini networks. Therefore, a 3D environment artist does not have to leave their familiar Houdini toolset anymore to make use of CityEngine’s procedural modeling power. Complicated export-import pipelines are no longer needed, which also means that the procedural building models do not need to be “baked” anymore. The buildings stay procedural during the entire modeling workflow (optionally even at render time). Consequently, the 3D environment artist can change the height, style and appearance of buildings easily with a parametric interface at any point during production.

Palladio requires so-called rule packages (RPK) as input, which are authored in CityEngine. An RPK includes assets and a CGA rule file which encodes an architectural style. Comprehensive RPK examples are available below and can be used “out-of-the-box” in Palladio.

Palladio is well suited for managing the procedural generation of architectural 3D content in digital sets. However, Palladio is restricted to the procedural generation of single buildings / objects. Palladio does not include the city layouting and street network editing tools of CityEngine i.e. the rich CityEngine toolset to design a city from scratch (or based on geographic data) is still needed.

## Quick Start

1. Download the latest palladio binary from the [release page](https://github.com/esri/palladio/releases).
1. Extract the archive into your `<home>\Documents\houdiniX.Y` (Windows) or `<home>/houdiniX.Y` (Linux) directory, so that the files end up in the corresponding `config`, `dso` and `packages` sub-directories.
1. Start Houdini and Palladio will be loaded automatically. 
1. In Houdini create an initial shape (i.e. a `grid` node with 2 cols/rows).
1. Open the geometry node in the network editor and connect the mesh output to a new `pldAssign` node.
1. Create a new `pldGenerate` node and connect the input to the ouput of the previous `pldAssign` node.
1. Make the `pldGenerate` node the active render node.
1. In CityEngine, download e.g. [Tutorial 9](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm) and export the "Parthenon" CGA rule to a RPK (see [Creating a Rule Package](https://doc.arcgis.com/en/cityengine/latest/help/help-rule-package.htm)).
1. Select the `pldAssign` node and set the `Rule Package` parameter in the prameter editor to the path of the previously exported RPK.
1. The model should generate and rule attributes should now be editable inside the parameter editor.

![](doc/img/parthenon01.png) 

## Table of Contents

* [User Manual](doc/usage.md)
* [Examples](https://esri.github.io/cityengine/palladio)
* [Developer Manual](doc/build.md)
* [Release Notes](doc/changelog.md)
* [Licensing Information](#licensing-information)

External documentation:
* [CityEngine Tutorials](https://doc.arcgis.com/en/cityengine/latest/tutorials/introduction-to-the-cityengine-tutorials.htm)
* [CityEngine CGA Reference](https://doc.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm)
* [CityEngine Manual](https://doc.arcgis.com/en/cityengine/latest/help/cityengine-help-intro.htm)
* [CityEngine SDK API Reference](https://esri.github.io/esri-cityengine-sdk/html/index.html)
* [Houdini HDK Reference](https://www.sidefx.com/docs/hdk)  

### Tips and Tricks

#### How to export an Alembic from CityEngine as input to Palladio

This short CityEngine guide explains how to include the random seed and start rule assignments when exporting input geometry to Alembic.

1. Run the following Python script on your scene objects to assign the random seed as object attributes: 
   ```python
   from scripting import *

   ce = CE()

   shapes = ce.getObjectsFrom(ce.scene(), ce.isShape)
   for s in shapes:
       seed = ce.getAttribute(s, "/ce/rule/randomSeed")
       ce.setAttribute(s, "seed", seed)
   ```
1. Assign this CGA script to the desired shapes to report out `seed` and `rule` (the start rule):
   ```
   version "2019.0"

   attr seed = 0

   Lot --> R	
   LotInner --> R
   Street --> R
   Sidewalk --> R
   Crossing --> R
   OpenSpace --> R
   Joint --> R
   R -->
       report("seed", seed)
       report("rule", initialShape.startRule)
   ```
    The `seed` rule attribute should have been connected to the previously assigned `seed` object attribute.
1. Export the shapes to Alembic, the two report values are now attached as user properties to the meshes.

#### Keep CityEngine Rule Packages in the Houdini Project

It can be useful to put RPKs into an `rpk` sub-directory of your current Houdini project. If you use `$HIP` to refer to the RPK in the pldAssign node, your Houdini project will stay relocatable.

#### Environment Variables

* `CITYENGINE_LOG_LEVEL`: controls the global (minimal) log level for all assign and generate nodes. Valid values are "debug", "info", "warning", "error", "fatal"
* `HOUDINI_DSO_ERROR`: useful to debug loading issues, see https://www.sidefx.com/docs/houdini/ref/env


## Community

Palladio is maintained as an open-source project by Matthias Buehler ([vrbn studios](https://www.vrbn.io)) and Simon Haegler (Esri R&D Zurich). Matthias is focusing on UX/UI topics and use-cases, while Simon is focusing on the implementation and procedural modelling technology.

Our goal is to bring as many Houdini and CityEngine users and developers on board as possible. Anyone is invited to submit issues and/or to fork and contribute to this project.

Join us on Slack at [palladio-houdini-ce.slack.com](https://palladio-houdini-ce.slack.com)!

Our thanks also go to the fine folks at Esri R&D Zurich and vrbn studios which provided valuable comments and feedback.


## History

Palladio has been invented by Matthias Buehler and Simon Haegler in early 2015. It started out as a research project at Esri R&D Zurich to get to a tighter integration of CityEngine and Houdini than just via file-based import/export of assets. Early results were promising, so we slowly cooked the project to v1.0 in our spare time. In December 2017, we decided to open-source Palladio with an Apache 2.0 license.


## Licensing Information

Palladio is free for personal, educational, and non-commercial use. Commercial use requires at least one commercial license of the latest CityEngine version installed in the organization. Redistribution or web service offerings are not allowed unless expressly permitted.

Palladio is under the same license as the included [CityEngine SDK](https://github.com/esri/cityengine-sdk#licensing). An exception is the Palladio source code (without CityEngine SDK, binaries, or object code), which is licensed under the Apache License, Version 2.0 (the “License”); you may not use this work except in compliance with the License. You may obtain a copy of the License at https://www.apache.org/licenses/LICENSE-2.0.

All content in the "Examples" directory/section is licensed under the APACHE 2.0 license as well.

The "Favela" example data is copyrighted by [vrbn studios](https://www.vrbn.io). Please contact vrbn_studios for commercial licensing options.

For questions or enquiries, please contact the Esri CityEngine team (cityengine-info@esri.com).
