# Debian package (debhelper + pybuild, see debian/). Needs Debian 13+ /
# Ubuntu 25.10+ with the build dependencies: `sudo apt build-dep .`
# dpkg-buildpackage writes the .deb (plus .changes / .buildinfo) to ../
deb:
	dpkg-buildpackage -us -uc -b

.PHONY: deb
