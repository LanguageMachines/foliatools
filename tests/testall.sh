# /bin/sh

\rm -f *.diff
\rm -f *.tmp
\rm -f *.out*

if [ "$my_bin" = "" ];
then
    export my_bin=/home/sloot/usr/local/bin
fi

for file in test2text testtxt testalto testcorrect testcorbig \
	    testhocr testidf testpage testabby testlangcat teststats \
	    testhyph testpm testclean testwordtranslate testmerge
do bash ./testone.sh $file
done
