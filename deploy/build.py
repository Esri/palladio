import argparse
import glob
import subprocess
import json
import shutil
import os
from pathlib import Path
from string import Template

# Dirs and Paths
SCRIPT_DIR = Path(__file__).resolve().parent
CFG_PATH = SCRIPT_DIR / f'installer_config.json'
RESOURCE_DIR = SCRIPT_DIR / f'resources'

VERSION_PATH = SCRIPT_DIR / f'../version.txt'

DEFAULT_BUILD_DIR = SCRIPT_DIR / f'../build/build_msi'

TEMPLATE_DIR = SCRIPT_DIR / f'templates'
DEPLOYMENT_PROPERTIES_TEMPLATE_PATH = TEMPLATE_DIR / 'deployment.properties.in'

WIX_TEMPLATE_PATH = TEMPLATE_DIR / f'palladio.wxs.in'
WIX_REG_SEARCH_TEMPLATE_PATH = TEMPLATE_DIR / f'palladio_registry_search.in'
WIX_DIRECTORY_TEMPLATE_PATH = TEMPLATE_DIR / f'palladio_directory.in'
WIX_FEATURE_TEMPLATE_PATH = TEMPLATE_DIR / f'palladio_feature.in'

PROJECT_NAME = 'Palladio'
PLATFORM = 'win64'
PACKAGE_VENDOR_ESCAPED = 'Esri R&amp;amp;D Center Zurich'


def get_palladio_version_string(includePre=False, build_version=''):
	version_map = {}
	delimiter = '.'
	with open(VERSION_PATH, 'r') as version_file:
		version_text = version_file.read()
		lines = version_text.splitlines()
		for line in lines:
			key_value = line.split(' ')
			if len(key_value) == 2:
				version_map[key_value[0]] = key_value[1]
	version_string = version_map['PLD_VERSION_MAJOR'] + delimiter + \
            version_map['PLD_VERSION_MINOR'] + \
            delimiter + version_map['PLD_VERSION_PATCH']
	if includePre:
		version_string += version_map['PLD_VERSION_PRE']
	if len(build_version) > 0:
		version_string += '+b' + build_version
	return version_string


def gen_installer_filename(build_version):
	return PROJECT_NAME + '-installer-' + get_palladio_version_string(True, build_version) + '.msi'


def rel_to_os_dir(path):
	return Path(path).absolute()


def clear_folder(path):
	if path.exists():
		shutil.rmtree(path)
	os.makedirs(path)


def run_wix_command(cmd, build_dir):
	process = subprocess.run(args=[str(i) for i in cmd], cwd=build_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          universal_newlines=True)
	print(process.args)
	print(process.stdout)
	print(process.stderr)


def wix_harvest(houdini_version, binaries_install_dir, build_dir):
	output_path = build_dir / f'houdini{houdini_version}.wxs'
	wxs_file = output_path
	wxs_var = f'-dPalladioInstallComponentDir{houdini_version}={binaries_install_dir}'
	heat_cmd = ['heat', 'dir', binaries_install_dir, '-dr', f'PalladioInstallComponentDir{houdini_version}', '-ag',
             '-cg', f'PalladioInstallComponent{houdini_version}', '-sfrag', '-template fragment', '-var', f'wix.PalladioInstallComponentDir{houdini_version}', '-srd', '-sreg', '-o', output_path]

	run_wix_command(heat_cmd, build_dir)

	return wxs_file, wxs_var


def wix_compile(sources, vars, build_dir):
	candle_cmd = ['candle', '-arch', 'x64',
               build_dir / 'palladio.wxs'] + sources + vars
	run_wix_command(candle_cmd, build_dir)

	objects = glob.glob(str(build_dir / '*.wixobj'))
	return objects


def wix_link(objects, vars, build_dir, installer_filename):
	light_cmd = ['light', f'-dWixUILicenseRtf={SCRIPT_DIR}\\resources\\license.rtf', f'-dInstallDir=', '-ext', 'WixUIExtension',
              '-cultures:en-us', '-ext', 'WixUtilExtension', '-o', f'{build_dir}\\' + installer_filename]
	light_cmd.extend(objects)
	light_cmd.extend(vars)
	run_wix_command(light_cmd, build_dir)


def create_installer(houdini_versions, binary_folders, build_dir, installer_filename):
	wxs_files = []
	wxs_vars = []
	for houdini_version_key in binary_folders:
		rel_path_string = binary_folders[houdini_version_key]
		major = houdini_versions[houdini_version_key]['major']
		minor = houdini_versions[houdini_version_key]['minor']

		binary_path = rel_to_os_dir(rel_path_string)

		print(binary_path, " ", houdini_version_key)
		wxs_file, wxs_var = wix_harvest(
			str(major) + str(minor), binary_path, build_dir)
		wxs_files.append(wxs_file)
		wxs_vars.append(wxs_var)

	wix_objects = wix_compile(wxs_files, wxs_vars, build_dir)
	wix_link(wix_objects, wxs_vars, build_dir, installer_filename)


def copy_binaries(binary_folders, build_dir):
	clear_folder(build_dir)

	valid_binaries = {}
	for binary_folder in binary_folders:
		src_path = binary_folders[binary_folder]
		if len(src_path) == 0:
			continue
		os_src_path = rel_to_os_dir(src_path)
		if os_src_path.exists():
			binary_path = build_dir / f'install' / Path(binary_folder)
			print("copy ", os_src_path, " to ", binary_path)
			shutil.copytree(os_src_path, binary_path)
			valid_binaries[binary_folder] = binary_path
	return valid_binaries


def fill_template(src_path, token_value_dict):
	result = ''
	with open(src_path, 'r') as file:
		src = Template(file.read())
		result = src.substitute(token_value_dict)
	return result


def fill_template_to_file(src_path, dest_path, token_value_dict):
	result = fill_template(src_path, token_value_dict)
	with open(dest_path, 'w') as outfile:
		outfile.write(result)


def fill_wix_template(houdini_versions, copied_binaries, build_dir):
	wix_reg_search = ''
	wix_directories = ''
	wix_features = ''

	for version in copied_binaries:
		version_dict = {
			'HOU_MAJOR': houdini_versions[version]['major'],
			'HOU_MINOR': houdini_versions[version]['minor'],
			'HOU_PATCH': houdini_versions[version]['patch']
		}
		newline = ''
		if len(wix_reg_search) > 0:
			newline = '\n'
		wix_reg_search += newline + \
			fill_template(WIX_REG_SEARCH_TEMPLATE_PATH, version_dict)
		wix_directories += newline + \
			fill_template(WIX_DIRECTORY_TEMPLATE_PATH, version_dict)
		wix_features += newline + \
			fill_template(WIX_FEATURE_TEMPLATE_PATH, version_dict)

	wix_properties = {
		'PROJECT_NAME': PROJECT_NAME,
		'PLD_VERSION_MMP': get_palladio_version_string(),
		'PACKAGE_VENDOR_ESCAPED': PACKAGE_VENDOR_ESCAPED,
		'RESOURCE_FOLDER': RESOURCE_DIR,
		'HOUDINI_REGISTRY_SEARCH': wix_reg_search,
		'HOUDINI_DIRECTORY': wix_directories,
		'PALLADIO_FOR_HOUDINI_FEATURE': wix_features
	}
	fill_template_to_file(WIX_TEMPLATE_PATH, build_dir /
	                      f'palladio.wxs', wix_properties)


def fill_deployment_properties_templates(installer_filename, build_dir, build_version):
	deployment_properties = {
		'PACKAGE_NAME': PROJECT_NAME,
		'PLD_VERSION_BASE': get_palladio_version_string(),
		'PLD_VERSION': get_palladio_version_string(True, build_version),
		'PACKAGE_FILE_NAME': installer_filename,
		'PLD_PKG_OS': PLATFORM
	}
	fill_template_to_file(DEPLOYMENT_PROPERTIES_TEMPLATE_PATH, build_dir /
                       f'deployment.properties', deployment_properties)


def parse_arguments(houdini_versions):
	parser = argparse.ArgumentParser(
		description='Build unified MSI installer for the Palladio Plugin')
	for houdini_version in houdini_versions:
		h_major = houdini_versions[houdini_version]['major']
		h_minor = houdini_versions[houdini_version]['minor']
		parser.add_argument('-h' + h_major + h_minor, '--' + houdini_version, default='',
		                    help='path to binary location of Palladio build for ' + houdini_version)

	parser.add_argument('-bv', '--build_version', default='',
	                    help='build version for current Palladio build')

	parser.add_argument('-bd', '--build_dir', default=str(DEFAULT_BUILD_DIR),
	                    help='build directory where installer files are generated')

	parsed_args = vars(parser.parse_args())
	binary_folders = {}
	for item in parsed_args:
		if item.__contains__('houdini'):
			binary_folders[item] = parsed_args[item]
	return binary_folders, parsed_args['build_version'], Path(parsed_args['build_dir']).resolve()


def main():
	with open(CFG_PATH, 'r') as config_file:
		installer_config = json.load(config_file)

	houdini_versions = installer_config['houdini_versions']

	binary_folders, build_version, build_dir = parse_arguments(houdini_versions)

	copied_binaries = copy_binaries(binary_folders, build_dir)
	installer_filename = gen_installer_filename(build_version)

	fill_deployment_properties_templates(
		installer_filename, build_dir, build_version)
	fill_wix_template(houdini_versions, copied_binaries, build_dir)

	create_installer(houdini_versions, copied_binaries,
	                 build_dir, installer_filename)


if __name__ == '__main__':
	main()
