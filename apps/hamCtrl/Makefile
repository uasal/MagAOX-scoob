## These lines should be used instead *once* it is cleared to update purepyindi2 on Velma
#TARGET=hamCtrl
#include ../../Make/pythonApp.mk

# The following are temp workarounds for pathing per the install so purepyindi2 items will work without updating the base version on Velma
APP=hamCtrl
PYTHON_SCRIPTS_PREFIX=$(shell python -c "import sysconfig; print(sysconfig.get_path('scripts'))")

all : install
.PHONY: all install

# The pathing will change back / the first lines commented out will be used instead. 
# Temp for using the updated purepyindi2 without updating the current one in the pathing of opt/MagAOX
install:
	sudo -H /opt/conda/envs/cv2/bin/python -c 'import purepyindi2' || (echo "Need purepyindi2 installed to $(shell which python)" && exit 1)
	sudo -H /opt/conda/envs/cv2/bin/python -c 'import xconf' || (echo "Need xconf installed to $(shell which python)" && exit 1)
	sudo -H /opt/conda/envs/cv2/bin/python -m pip install -e .
	sudo -H ln -sfv $(PYTHON_SCRIPTS_PREFIX)/$(APP) /opt/MagAOX/bin/$(APP)
	@echo "*** Install succeeded, app located in /opt/MagAOX/bin/$(APP) ***"
