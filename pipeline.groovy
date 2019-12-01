// This pipeline is designed to run on Esri-internal CI infrastructure.

import groovy.transform.Field
import com.esri.zrh.jenkins.PipelineSupportLibrary 
import com.esri.zrh.jenkins.JenkinsTools
import com.esri.zrh.jenkins.ce.CityEnginePipelineLibrary
import com.esri.zrh.jenkins.ce.PrtAppPipelineLibrary

@Field def psl = new PipelineSupportLibrary(this)
@Field def cepl = new CityEnginePipelineLibrary(this, psl)
@Field def papl = new PrtAppPipelineLibrary(cepl)


// -- GLOBAL DEFINITIONS

@Field final String REPO         = 'git@github.com:Esri/palladio.git'
@Field final String SOURCE       = "palladio.git/src"
@Field final String BUILD_TARGET = 'package'

@Field final List CONFIGS_HOUDINI_165 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '16.5' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '16.5' ],
]

@Field final List CONFIGS_HOUDINI_170 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '17.0' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '17.0' ],
]

@Field final List CONFIGS_HOUDINI_175 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '17.5' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '17.5' ],
]

@Field final List CONFIGS_HOUDINI_180 = [
	[ os: cepl.CFG_OS_RHEL7, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_GCC63, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.0' ],
	[ os: cepl.CFG_OS_WIN10, bc: cepl.CFG_BC_REL, tc: cepl.CFG_TC_VC141, cc: cepl.CFG_CC_OPT, arch: cepl.CFG_ARCH_X86_64, houdini: '18.0' ],
]

// -- PIPELINE

@Field String myBranch = env.BRANCH_NAME

// entry point for standalone pipeline
def pipeline(String branchName = null) {
	cepl.runParallel(getTasks(branchName))
}

// entry point for embedded pipeline
Map getTasks(String branchName = null) {
	if (branchName)
		myBranch = branchName

	Map tasks = [:]
	tasks << taskGenPalladio()
	return tasks
}


// -- TASK GENERATORS

Map taskGenPalladio() {
    Map tasks = [:]
    // FIXME: this is a workaround to get unique task names
	tasks << cepl.generateTasks('pld-hdn16.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_165)
	tasks << cepl.generateTasks('pld-hdn17.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_170)
	tasks << cepl.generateTasks('pld-hdn17.5', this.&taskBuildPalladio, CONFIGS_HOUDINI_175)
	tasks << cepl.generateTasks('pld-hdn18.0', this.&taskBuildPalladio, CONFIGS_HOUDINI_180)
	return tasks;
}


// -- TASK BUILDERS

def taskBuildPalladio(cfg) {
	List deps = [] // empty dependencies = by default use conan packages
	
	List defs = [
		[ key: 'HOUDINI_USER_PATH',   val: "${env.WORKSPACE}/install" ],
		[ key: 'PLD_VERSION_BUILD',   val: env.BUILD_NUMBER ],
		[ key: 'PLD_HOUDINI_VERSION', val: cfg.houdini]
	]
	
	// pipeline params tell us if we need to build with a development version of CESDK
	if (params.PRM_CESDK_BRANCH) {
		// redirect the CESDK dependency to the right internal branch build
		Map myCESDK = PrtAppPipelineLibrary.Dependencies.CESDK.clone()
		myCESDK.g = { return params.PRM_CESDK_BRANCH.replace('/', '.') }
		deps << myCESDK

		defs << [ key: 'PLD_CONAN_CESDK_DIR', val: myCESDK.p ]
	}
	
	papl.buildConfig(REPO, myBranch, SOURCE, BUILD_TARGET, cfg, deps, defs)
	
	def versionExtractor = { p ->
		def vers = (p =~ /.*palladio-(.*)\.hdn.*/)
		return vers[0][1]
	}
	def classifierExtractor = { p ->
		def cls = (p =~ /.*palladio-.*\.(hdn.*)-(windows|linux)\..*/)
		return cls[0][1] + '.' + cepl.getArchiveClassifier(cfg)
	}
	papl.publish('palladio', myBranch, "palladio-*", versionExtractor, cfg, classifierExtractor)
}


// -- make embeddable

return this