all:
	##################################################
	###               kissat_mab                   ###
	##################################################
	chmod a+x kissat_mab/configure #kissat_mab/scripts/*.sh
	cd kissat_mab && ./configure --compact
	+ $(MAKE) -C kissat_mab

	##################################################
	###               MapleCOMSPS                  ###
	##################################################
	if [ -d mapleCOMSPS/m4ri-20140914 ]; then : ; \
	else cd mapleCOMSPS && tar zxvf m4ri-20140914.tar.gz && \
	cd m4ri-20140914 && ./configure; fi
	+ $(MAKE) -C mapleCOMSPS/m4ri-20140914
	+ $(MAKE) -C mapleCOMSPS r


	##################################################
	###                 PaInleSS                   ###
	##################################################
	+ $(MAKE) -C painless-src
	mv painless-src/painless painless

clean:
	##################################################
	###                   Clean                    ###
	##################################################
	+ $(MAKE) clean -C painless-src
	+ $(MAKE) clean -C kissat_mab -f makefile.in
	rm -rf kissat_mab/build
	rm -rf mapleCOMSPS/m4ri-20140914
	+ $(MAKE) -C mapleCOMSPS clean
	rm -f painless
