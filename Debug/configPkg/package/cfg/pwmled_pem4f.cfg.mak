# invoke SourceDir generated makefile for pwmled.pem4f
pwmled.pem4f: .libraries,pwmled.pem4f
.libraries,pwmled.pem4f: package/cfg/pwmled_pem4f.xdl
	$(MAKE) -f /Users/jmascavage/cc_workspace_v11/werewolf_TM4C123GXL/src/makefile.libs

clean::
	$(MAKE) -f /Users/jmascavage/cc_workspace_v11/werewolf_TM4C123GXL/src/makefile.libs clean

