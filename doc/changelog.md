# Palladio ChangeLog

## v1.8.0 Beta 1 (Aug 23, 2020)
* Update default CityEngine SDK to 2.2 (CityEngine 2020.0) (#146).
* Added support for CGA array attributes (#146).
* Added per-node control of log level (#30).
* At startup, print Palladio version on console if log level is "info" or lower.
* Development
  * On Windows, building Palladio now requires MSVC 14.2 (#146).
  * Various code cleanups and adoption of clang-format rules from [Serlio](https://github.com/esri/serlio).
  * Pruned releasing for Houdini 17.0 (you can still build yourself for 17.0).
  * Relaxed Python/Conan version requirements.

## v1.7.0 (June 14, 2020)
* No functional changes compared to Beta 1 

## v1.7.0 Beta 1 (Dec 9, 2019)
* Added support for Houdini 18 and removed Houdini 16 (#143).
* Updated built artifacts to latest Houdini versions (#139).
* Filter unnecessary components of PRT from release (#141).
* Fixed pipeline to not install files outside of the install directory (#138).

## v1.7.0 Pre-Release (Oct 20, 2019)
* Improved setting start rule from primitive attribute (i.e. do not prepend style to start rule if it is already present).
* Improved installation experience (avoid setting OS PATH on Windows).
* Update default CityEngine SDK version to 2.1 (CityEngine 2019.1).
* Only pass non-default rule attribute values to PRT. This fixes setting inter-dependent rule attributes.

## v1.6.3 (July 27, 2019)
* Optimized cooking time of pldGenerate (e.g. Parthenon example from CityEngine tutorial 9 cooks 7x faster)
* Added "tips and tricks" section with guide how to export Alembics from CityEngine for Palladio

## v1.6.2 (June 22, 2019)
* Fixed broken texture coordinates in the generated Houdini geometry in case not all PRT meshes had any (#118).

## v1.6.1 (June 7, 2019)
* Added support for texture coordinates on incoming initial shapes (#112).
* Assign SOP: do not overwrite main attributes like pldStartRule if they are already present on the incoming geometry (#111).
* Assign SOP: fixed rule attribute default value evaluation (each primitive can have different main attributes) (#111).
* Fixed support for PolygonSoup primitives as initial shapes (#110).

## v1.6.0 (May 8, 2019)
* Added support for PRT 2
* Removed technical requirement for CityEngine license.
* Added support for new PRT PBR material attributes.
* Various build system improvements (supports both PRT 1.x and 2.x).

## v1.5.0 (May 7, 2019)
* No functional change to beta 2.

## v1.5.0 Beta 2 (Mar 19, 2019)
* Fixed crash in pldAssign node if no RPK is set.

## v1.5.0 Beta 1 (Mar 14, 2019)
* Added support for Rule Packages (RPK) embedded in Houdini Digital Assets (HDA)
* Added support for Houdini 17.5

## v1.4.1 (Mar 11, 2019)
* Always use UV set 0 as a fallback (mimics CityEngine behavior).
* Internal cleanup: Simplified UV coordinate transport between PRT and Palladio.

## v1.4.0 (Feb 27, 2019)
* Generated material primitive attributes now use the "generic names" instead of the "CGA names" (see PRT reference for file prtx/Material.h for details).
* Fixed wrong texture paths for assets that originated in another CityEngine project (e.g. ESRI.lib) at RPK creation time.

## v1.3.1 (Feb 20, 2019)
* Added a workaround to prevent a crash in PRT 1.9 when using an invalid CGB path in the assign node.

## v1.3.0 (Jan 20, 2019)
* Update build system to support both Houdini 16.5 and 17.0.
* For Houdini 17, use bundled boost libraries.

## v1.2.0 (Jan 17, 2019)
* Build System changes to improve continuous integration support (Jenkins).
* Updated required cmake version to 3.13 (preparation for Houdini 17 support).
* Added pipenv script for easier access to conan.
* Reduced Houdini package size.

## v1.2.0 RC 1 (Jun 11, 2018)
* Support for inter-shape occlusion queries (#33).

## v1.1.1 (Apr 16, 2018)
* Fixed asset loading issues in Windows build (#88).

## v1.1 (Apr 7, 2018)
* Added support for Windows (#85).
* Raised minimal Houdini version to 16.5.x (#85).
* Added support to reload RPKs (#58).

## v1.0 (Jan 26, 2018)
* Renamed project to "Palladio" (#68).
* Updated documentation (#38).
* Published demo scene (#74).

## v0.9 (Jan 18, 2018)
* Add support for writing out primitive attributes from rule/generic CGA attributes (#40).
* Removed random seed parameter UI, read it from incoming primitive attributes instead. Added centroid-based random seed as fallback.
* Correctly update assign node parameters from RPK (#62).
* Make primitive group creation optional and based on primitive classifier (#67).
* Add support for CityEngine network licensing (#41).
* Fixed crash when handling invalid RPK paths.
* Remove style prefix on primitive attribute names.

## v0.8 (Dec 19, 2017)
* Added support for multiple UV sets (#36).
* Added support for CGA reports (#39).
* Properly use Houdini input mesh connectivity (make CGA operation "isClosedSurface" work correctly) (#49).
* Refactored assign node to correctly use incoming geometry to evaluate default rule attribute values.

## v0.7.1 (Nov 27, 2017)
* Proper support for UVs and material attributes.
* Introduced Conan to handle 3rd party packages in the build system (Houdini, CityEngine).
* Updated to CityEngine SDK 2017.1 (CE SDK 1.9).

## v0.7 (Sep 19, 2017)
* Split plugin into two SOPs ("assign" and "generate") to support proper evaluation of rule attribute default values.
* Abandoned rule param UI, purely rely on Houdini attribute tools (e.g. "AttributeCreate") to create/modify rule attributes (= no need for complicated UI on assign node).
* Updated to Houdini 16 and CityEngine 2017.
* Dropped Windows/macOS support temporarily.

## v0.6 (Mar 15, 2016)
* Support for rule attribute UI via Houdini multi-parameter.
* Experimental support for Windows/macOS.
* Updated to Houdini 15 and CityEngine 2016.

## v0.4 (Oct 22, 2015)
* Working geometry generation from CityEngine rule packages
* Support to read rule attributes from primitive attributes
* Implemented rule execution as single surface operator (SOP).
* Updated to Houdini 13 and CityEngine 2015.

## v0.1 - v0.3 (Spring 2015)
* Early experiments with Houdini 12 and CityEngine 2014.
