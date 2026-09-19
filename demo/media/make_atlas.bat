@pushd %~dp0
@set IMAGE_SIZE=128
@set ATLAS_NAME=..\src\tftf_terrain_atlas.h
@set INPUT_DIR=island_terrain_tiles

python ..\..\media\make_atlas.py --no-embed-png %INPUT_DIR% %IMAGE_SIZE% %ATLAS_NAME% --symbol-prefix TFTF --var-prefix tftf
popd
