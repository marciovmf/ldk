pushd %~dp0
python %P%make_atlas.py png ..\src\editor\ldk_editor_atlas.h 64
popd
