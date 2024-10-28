eir_uefi_sources = ['entry.cpp', 'debug.cpp']
inc = include_directories('../../../klibc', '../../generic', '../../../common', '../x86')

# HACK
freestnd_c_hdrs_dep = dependency(
	'freestnd-c-hdrs-' + host_machine.cpu_family(),
	required: true,
	fallback: ['freestnd-c-hdrs', 'freestnd_c_hdrs_dep'],
)

freestnd_cxx_hdrs_dep = dependency(
	'freestnd-cxx-hdrs-' + host_machine.cpu_family(),
	required: true,
	fallback: ['freestnd-cxx-hdrs', 'freestnd_cxx_hdrs_dep'],
)

executable('eir-uefi', eir_uefi_sources + eir64_sources,
	include_directories : [inc, eir_includes],
	dependencies : [freestnd_cxx_hdrs_dep, frigg, eir_dependencies],
	c_args: eir_c_args,
	cpp_args: eir_cpp_args,
	install : true
)
