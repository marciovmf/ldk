@echo ================== GENERATING TILES =====================
@pushd %~dp0
python make_tileset.py tileset_assets\config.json tileset_assets\generated && make_atlas.bat
@popd
