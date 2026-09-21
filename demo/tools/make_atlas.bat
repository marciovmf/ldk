@echo ================== GENERATING ATLAS =====================
@pushd %~dp0
@set IMAGE_SIZE=160
@set OUTPUT_DIR=..\src\system
@set HEADER_NAME=%OUTPUT_DIR%\tftf_terrain_atlas.h
@set ATLAS_NAME=%OUTPUT_DIR%\tftf_terrain_atlas.png
@set INPUT_DIR=tileset_assets\generated

python ..\..\media\make_atlas.py --no-embed-png %INPUT_DIR% %IMAGE_SIZE% %HEADER_NAME% --symbol-prefix TFTF --var-prefix tftf && ^
move %ATLAS_NAME% ..\runtree\assets\textures\
@popd

