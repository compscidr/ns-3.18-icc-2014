# script which generates graphs given two csv files from different
# experiments for comparison

# assumes both files have the same number of lines

if [ $# -ne 2 ]
then
  echo "Usage $0 <file1> <file2>";	# not two parameters
  exit 2;
fi

count=1;
param="p";
inputfile1=$(basename "$1");
inputfile2=$(basename "$2");

inputdir1=$(dirname "$1");
inputname1=$(basename "$inputdir1");

inputdir2=$(dirname "$2");
inputname2=$(basename "$inputdir2");

awk -F"," -v name="$inputname1" '{ $1=1; print; }' $1 > temp1.txt;
awk -F"," -v name="$inputname2" '{ $1=2; print; }' $2 > temp2.txt;

while read f1; do
	filename1=$count-$inputname1-$inputfile1;
	filename2=$count-$inputname2-$inputfile2;
	echo $f1 > $filename1;
	sed -n "$count$param" temp2.txt > $filename2;
	export name1=${filename1%.*};
	export name2=${filename2%.*};
	export label1=$inputname1;
	export label2=$inputname2;
	export title=$count-${inputfile1%.*};
	./plot2.sh
	count=$((count+1));
done < temp1.txt


#while read f1; do
#  filename=$count$inputfile;
#  echo $f1 > $filename1;
#  echo $inputname1 $inputname2;
#  
#  sed -n "$count$param" $2 >> $filename;
#  export name1=${filename1%.*};
#  ./plot2.sh
#  
#done < $1
