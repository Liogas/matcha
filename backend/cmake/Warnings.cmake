# Avertissements du compilateur pour tout le code compilé dans ce dossier et
# ses sous-dossiers (src/, tests/, libs/). -Werror n'est pas ici : ce sont les
# presets qui l'activent, via CMAKE_COMPILE_WARNING_AS_ERROR.
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
	# GCC 12 en Release : faux positif dans picojson (inclus par jwt-cpp),
    # signalé malgré -isystem car le code est inliné dans le nôtre.
	$<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
)