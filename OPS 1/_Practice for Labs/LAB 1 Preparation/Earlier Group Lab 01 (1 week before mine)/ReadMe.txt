################# ReadMe 1 #################

maybe ; fgets + strcmp of length equal to -I arg would be better than using fgetc for -I 




################# ReadMe 2 #################
Now that I think about it, my global variables do meet these conditions!
GLOBAL VARIABLES ARE USUALLY ONLY ALLOWED FOR VALUES/VARIABLES YOU HAVE TO RETURN TO THE MAIN FUNCTION FROM NFTW
AND SOME OTHER SCENARIOS THAT WILL BE DISCUSSED IN TUTORIAL NOTES!


TEACHER IN TUTORIAL 1:
lobal variables are known to be the root of all evil in coding. 
They are very easy to use and have many negative consequences on 
code portability, re-usability and last but not least on how easily 
you can understand the code. They create indirect code dependencies 
that are sometimes really hard to trace. 
Unfortunately there are exceptional situations where we are forced to 
use them. This is one of such a cases because nftw callback 
function has no better way to return its results to the main code. 
Other exceptions will be discussed soon. 
You are allowed to use globals ONLY in those defined cases.


#UPDATE: NOT REALLY!!--MAYBE - Could've probably used FTW_DP flag instead of using Global Vars to store largest filesize.txt