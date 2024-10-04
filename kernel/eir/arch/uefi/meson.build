eir_uefi_sources = ['entry.cpp', 'debug.cpp', '../../generic/main.cpp', '../../../common/libc.cpp']
inc = include_directories('../../../klibc', '../../generic', '../../../common')

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

executable('eir-uefi', eir_uefi_sources,
	include_directories : inc,
	dependencies : [frigg, freestnd_cxx_hdrs_dep],
	install : true
)
