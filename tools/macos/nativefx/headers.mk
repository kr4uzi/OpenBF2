# [CHOSEN, not recovered] Generate native library prerequisites only; no oracle executable.
.PHONY: nativefx-headers
nativefx-headers: $(widl_headers) include/private/spirv_grammar.h include/private/vkd3d_version.h
