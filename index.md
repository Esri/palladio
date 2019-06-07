---
title: Palladio - CityEngine Plugin for Houdini
---

<table>
<tr>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="#downloads"><img src="images/cog.png" width="100px" /> Downloads</a></td>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="#examples"><img src="images/bldg3d.png" width="100px" /> Examples</a></td>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="#documentation"><img src="images/docs.png" width="100px" /> Documentation</a></td>
</tr>
</table>

<img src="images/Palladio_icon_v1_512.png" width="110" align="right" />Palladio is a plugin for [SideFX Houdini](https://www.sidefx.com). It provides operator nodes which enable the execution of [Esri CityEngine](http://www.esri.com/software/cityengine) ‘rules’ within Houdini networks. Therefore, a 3D environment artist does not have to leave their familiar Houdini toolset anymore to make use of CityEngine’s procedural modeling power. Complicated export-import pipelines are no longer needed, which also means that the procedural building models do not need to be “baked” anymore. The buildings stay procedural during the entire modeling workflow (optionally even at render time). Consequently, the 3D environment artist can change the height, style and appearance of buildings easily with a parametric interface at any point during production.

Palladio requires so-called rule packages (RPK) as input, which are authored in CityEngine. An RPK includes assets and a CGA rule file which encodes an architectural style. Comprehensive RPK examples are available below and can be used “out-of-the-box” in Palladio.

Palladio is well suited for managing the procedural generation of architectural 3D content in digital sets. However, Palladio is restricted to the procedural generation of single buildings / objects. Palladio does not include the city layouting and street network editing tools of CityEngine i.e. the rich CityEngine toolset to design a city from scratch (or based on geographic data) is still needed.

*Palladio is free for non-commercial use.* Commercial use requires at least one commercial license of the latest CityEngine version installed in the organization. No redistribution is allowed. Please refer to the licensing section below for more detailed licensing information.


## Downloads

| ![Linux](images/linux.png)   | [Palladio 1.6.0, Houdini 17.5, PRT 2.0](https://github.com/Esri/palladio/releases/download/v1.6.0/palladio-1.6.0+b178-hdn17-5-229.prt2-0-5403.rhel7-gcc63-x86_64-rel-opt.tgz) |
| ![Windows](images/win32.png) | [Palladio 1.6.0, Houdini 17.5, PRT 2.0](https://github.com/Esri/palladio/releases/download/v1.6.0/palladio-1.6.0+b178-hdn17-5-229.prt2-0-5403.win10-vc141-x86_64-rel-opt.zip) |

More binaries (for older Houdini versions) are available on the [github release page](https://github.com/Esri/palladio/releases).


## Examples

<table>

<tr><td colspan="2"><h3>Angkor Thom</h3></td></tr>
<tr>
	<td width="420px">
		<a href="images/AngkorThom/angkorThom_1.jpg"><img src="images/AngkorThom/angkorThom_1.jpg" width="400" /></a>
		<a href="images/AngkorThom/Angkor_Thom_CityEngine_Scene.png"><img src="images/AngkorThom/Angkor_Thom_CityEngine_Scene.png" width="400" /></a>
		<a href="images/AngkorThom/Angkor_Thom_Houdini_Scene.png"><img src="images/AngkorThom/Angkor_Thom_Houdini_Scene.png" width="400" /></a>
	</td>
	<td>
		<p>This example includes full CityEngine and Houdini projects. It demonstrates how to use CityEngine to author procedural assets like Walls, Stairs, Porticos and combine them in Houdini via Palladio and Rule Packages. The scene consists of three major steps:
		<ol>
			<li>The "/obj/PALLADIO_BASE" geometry node generates the various CityEngine Rules via Palladio and assigns them to multiple outputs.</li>
			<li>Multiple "/obj/process_palladio_xxx" nodes reference above outputs, apply a stone weathering effect and cache out the geometry.</li>
			<li>The "/obj/creepers_xxx" nodes apply some creeper plant effects.</li>
		</ol>
		</p>

		<p>Downloads:</p>
		<ul>
			<li><a href="https://github.com/Esri/palladio/releases/download/v1.6.0/Palladio_Example_Angkor_Thom_NC_v1.zip">Houdini and CityEngine Projects</a></li>
		</ul>

		<p>Notes:</p>
		<ul>
			<li>Created by Matthias Buehler</li>
			<li>Also covered in "Digital Production" <a href="https://www.digitalproduction.com/ausgabe/digital-production-05-2018/">Issue 05/18</a> (Article in German).</li>
		</ul>
	</td>
</tr>

<tr><td colspan="2"><h3>Favela</h3></td></tr>
<tr>
	<td>
		<a href="images/Favela/favela.mantra_ipr.0001.jpg"><img src="images/Favela/favela.mantra_ipr.0001.jpg" width="400" /></a>
		<a href="images/Favela/houdini_favela_scene.png"><img src="images/Favela/houdini_favela_scene.png" width="400" /></a>
		<a href="images/Favela/houdini_favela_hda_params.png"><img src="images/Favela/houdini_favela_hda_params.png" width="400" /></a>
	</td>
	<td>
		<p>The Favela example demonstrates the packaging of a complex Rule Package into a Houdini Digital Asset with custom high-level parameters. The provided Houdini scene additionally shows how to prepare the input geometry for Palladio out of an Alembic archive (setup of random seed and start rule).</p>

		<p>Downloads:</p>
		<ul>
			<li><a href="https://github.com/Esri/palladio/releases/download/v1.6.1/favela_houdini_project_v1.zip">Houdini Project</a> with HDA and pre-cached geometry included.</li>
			<li><a href="https://github.com/Esri/palladio/releases/download/v1.6.0/FAVELA.rpk">Favela RPK</a></li>
		</ul>

		<p>Notes:</p>
		<ul>
			<li>Requires Houdini 17.5 and Palladio 1.6.1 or later.</li>
			<li>Based on the original <a href="https://www.youtube.com/watch?v=IY5gU1J39Ec">"Favela" project (YouTube)</a> created with CityEngine, Maya and Maxwell.</li>
			<li>Created by Matthias Buehler</li>
		</ul>
	</td>
</tr>

<tr><td colspan="2"><h3>CityEngine Material Handling (Mantra)</h3></td></tr>
<tr>
	<td>
		<img src="https://user-images.githubusercontent.com/1630632/53511493-e44e9d80-3ac0-11e9-8578-a6fc2093d312.png" width="400" />
	</td>
	<td>
		<p>This example shows how to use the generated CityEngine material attributes with Mantra.</p>

		<p>Downloads:</p>
		<ul>
		<li><a href="https://github.com/Esri/palladio/releases/download/v1.4.0/PalladioDemo03.zip">Houdini Scene (HIPNC)</a></li>
		<li><a href="https://github.com/Esri/palladio/releases/download/v1.6.1/PalladioCityEngineMaterial-v2.hdanc">Houdini Digital Asset (HDANC)</a></li>
		</ul>
	</td>
</tr>

<tr><td colspan="2"><h3>Computer Case Graveyard</h3></td></tr>
<tr>
	<td>
		<img src="https://user-images.githubusercontent.com/1630632/38602293-9e48da0c-3d6a-11e8-88b9-bb930d7b6a38.jpg" width="400" />
	</td>
	<td>
		<a href="https://github.com/Esri/palladio/releases/download/v1.1/PalladioDemo02.zip">Houdini Scene (HIPNC)</a>
	</td>
</tr>

<tr><td colspan="2"><h3>Palladio Basic Use Case</h3></td></tr>
<tr>
	<td>
		<img src="https://user-images.githubusercontent.com/1630632/35410758-ae41d858-0216-11e8-8ba6-286dc87f532a.png" width="400" />
	</td>
	<td>
		<a href="https://github.com/Esri/palladio/releases/download/v1.0/PalladioDemo01.zip">Houdini Scene (HIPNC)</a>
	</td>
</tr>

</table>


## Documentation

* [Quick Start](https://github.com/Esri/palladio/blob/master/doc/usage.md)
* [ChangeLog](https://github.com/Esri/palladio/blob/master/doc/changelog.md)
* More documentation is available on the main [README.md](https://github.com/Esri/palladio/blob/master/README.md)

External documentation:
* [CityEngine CGA Reference](http://desktop.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm)
* [CityEngine Manual](https://doc.arcgis.com/en/cityengine/latest/help/cityengine-help-intro.htm)
* [CityEngine Tutorials](http://desktop.arcgis.com/en/cityengine/latest/tutorials/introduction-to-the-cityengine-tutorials.htm)


## Licensing

Palladio is under the same license as the included [CityEngine SDK](https://github.com/Esri/esri-cityengine-sdk#licensing).

An exception is the Palladio source code (without CityEngine SDK, binaries, or object code), which is licensed under the Apache License, Version 2.0 (the “License”); you may not use this work except in compliance with the License. You may obtain a copy of the License at [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0).

The "Favela" example data is copyrighted by [vrbn studios](https://www.vrbn.io). Please contact vrbn_studios for commercial licensing options.
