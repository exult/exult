@if "%1"=="--skipflexes" @goto usecode
@expack.exe > NUL: 2>&1
@if errorlevel 9009 (
	@echo expack was not found; please add it to your PATH environment variable
	@echo and run this script again.
	@echo.
	@exit
)

@for %%f in (faces gumps shapes paperdol sprites fonts mainshp) do @if exist "graphics\%%f.in" (
	@echo Generating %%f...
	@expack.exe -i graphics/%%f.in > NUL: 2>&1
	@del ..\data\%%f_*.h > NUL: 2>&1
)
:usecode
@if exist usecode.uc (
	@ucc.exe -o usecode %0 > NUL: 2>&1
	@if errorlevel 9009 (
		@echo ucc was not found; please add it to your PATH environment variable
		@echo and run this script again.
		@echo.
		@exit
	)

	@echo Compiling Usecode...
	@ucc.exe -o ../data/usecode usecode.uc
	@if errorlevel 1 (
		@echo There were error^(s^) compiling usecode!
		@pause
	) else (
		@echo Usecode has been successfully compiled!
	)
)
