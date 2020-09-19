# script for performing data analysis
# Jason Ernst, May 2013 - University of Guelph
# Source: http://blog.thinkoriginally.com/2011/06/14/gnuplot-box-and-whiskers-plot/

# First column (avg throughput)
count=1;
filename="Average Throughput.csv";
for fn in `ls *tr.txt | sort -V`;
do
	sort $fn -o $fn		# first we sort the data so we can easily find min, median, max
	awk -F, -v count="$count" -v filename="$filename" '{ 
		s+= $1; 
		array[NR]=$1 
	} END {
		for(x=1;x<=NR;x++) {
			sumsq+=((array[x]-(s/NR))^2);
		} 
		if(NR % 2) {
			median = array[(NR + 1) / 2];
		} else {
			median = (array[(NR / 2)] + array[(NR / 2) + 1]) / 2.0;
		}
		if(NR % 4)
		{
			first = (array[int(NR / 4)] + array[int(NR / 4) + 1]) / 2.0;
			third = (array[int(NR / 4 * 3)] + array[int(NR / 4*3)+1]) / 2.0;
		} else {
			first = array[NR / 4];
			third = array[NR / 4 * 3];
		}
		#print "sum:",s;
		#print "average:",s/NR;
		#print "first quartile:",first;
		#print "median:",median;
		#print "third quartile:",third;
		#print "low:",array[1];
		#print "high:",array[NR];
		#print "std dev:",sqrt(sumsq/NR);
		#print "samples:",NR; 
		print count*2 "," array[1] "," first "," median "," third "," array[NR] >> filename;
	}' $fn
	count=$((count+1));
done

# 2nd column (total throughput)
count=1;
filename="Total Throughput.csv";
for fn in `ls *tr.txt | sort -V`;
do
	sort $fn -o $fn		# first we sort the data so we can easily find min, median, max
	awk -F, -v count="$count" -v filename="$filename" '{ 
		s+= $2; 
		array[NR]=$2 
	} END {
		for(x=1;x<=NR;x++) {
			sumsq+=((array[x]-(s/NR))^2);
		} 
		if(NR % 2) {
			median = array[(NR + 1) / 2];
		} else {
			median = (array[(NR / 2)] + array[(NR / 2) + 1]) / 2.0;
		}
		if(NR % 4)
		{
			first = (array[int(NR / 4)] + array[int(NR / 4) + 1]) / 2.0;
			third = (array[int(NR / 4 * 3)] + array[int(NR / 4*3)+1]) / 2.0;
		} else {
			first = array[NR / 4];
			third = array[NR / 4 * 3];
		}
		#print "sum:",s;
		#print "average:",s/NR;
		#print "first quartile:",first;
		#print "median:",median;
		#print "third quartile:",third;
		#print "low:",array[1];
		#print "high:",array[NR];
		#print "std dev:",sqrt(sumsq/NR);
		#print "samples:",NR; 
		print count*2 "," array[1] "," first "," median "," third "," array[NR] >> filename;
	}' $fn
	count=$((count+1));
done

# 3rd column (avg delay)
count=1;
filename="Average Delay.csv";
for fn in `ls *tr.txt | sort -V`;
do
	sort $fn -o $fn		# first we sort the data so we can easily find min, median, max
	awk -F, -v count="$count" -v filename="$filename" '{ 
		s+= $3; 
		array[NR]=$3 
	} END {
		for(x=1;x<=NR;x++) {
			sumsq+=((array[x]-(s/NR))^2);
		} 
		if(NR % 2) {
			median = array[(NR + 1) / 2];
		} else {
			median = (array[(NR / 2)] + array[(NR / 2) + 1]) / 2.0;
		}
		if(NR % 4)
		{
			first = (array[int(NR / 4)] + array[int(NR / 4) + 1]) / 2.0;
			third = (array[int(NR / 4 * 3)] + array[int(NR / 4*3)+1]) / 2.0;
		} else {
			first = array[NR / 4];
			third = array[NR / 4 * 3];
		}
		#print "sum:",s;
		#print "average:",s/NR;
		#print "first quartile:",first;
		#print "median:",median;
		#print "third quartile:",third;
		#print "low:",array[1];
		#print "high:",array[NR];
		#print "std dev:",sqrt(sumsq/NR);
		#print "samples:",NR; 
		print count*2 "," array[1] "," first "," median "," third "," array[NR] >> filename;
	}' $fn
	count=$((count+1));
done

# 4th column (avg pdr)
count=1;
filename="Average Packet Delivery Ratio.csv";
for fn in `ls *tr.txt | sort -V`;
do
	sort $fn -o $fn		# first we sort the data so we can easily find min, median, max
	awk -F, -v count="$count" -v filename="$filename" '{ 
		s+= $4; 
		array[NR]=$4 
	} END {
		for(x=1;x<=NR;x++) {
			sumsq+=((array[x]-(s/NR))^2);
		} 
		if(NR % 2) {
			median = array[(NR + 1) / 2];
		} else {
			median = (array[(NR / 2)] + array[(NR / 2) + 1]) / 2.0;
		}
		if(NR % 4)
		{
			first = (array[int(NR / 4)] + array[int(NR / 4) + 1]) / 2.0;
			third = (array[int(NR / 4 * 3)] + array[int(NR / 4*3)+1]) / 2.0;
		} else {
			first = array[NR / 4];
			third = array[NR / 4 * 3];
		}
		#print "sum:",s;
		#print "average:",s/NR;
		#print "first quartile:",first;
		#print "median:",median;
		#print "third quartile:",third;
		#print "low:",array[1];
		#print "high:",array[NR];
		#print "std dev:",sqrt(sumsq/NR);
		#print "samples:",NR; 
		print count*2 "," array[1] "," first "," median "," third "," array[NR] >> filename;
	}' $fn
	count=$((count+1));
done

# 5th column (avg jitter)
count=1;
filename="Average Jitter.csv";
for fn in `ls *tr.txt | sort -V`;
do
	sort $fn -o $fn		# first we sort the data so we can easily find min, median, max
	awk -F, -v count="$count" -v filename="$filename" '{ 
		s+= $5; 
		array[NR]=$5 
	} END {
		for(x=1;x<=NR;x++) {
			sumsq+=((array[x]-(s/NR))^2);
		} 
		if(NR % 2) {
			median = array[(NR + 1) / 2];
		} else {
			median = (array[(NR / 2)] + array[(NR / 2) + 1]) / 2.0;
		}
		if(NR % 4)
		{
			first = (array[int(NR / 4)] + array[int(NR / 4) + 1]) / 2.0;
			third = (array[int(NR / 4 * 3)] + array[int(NR / 4*3)+1]) / 2.0;
		} else {
			first = array[NR / 4];
			third = array[NR / 4 * 3];
		}
		#print "sum:",s;
		#print "average:",s/NR;
		#print "first quartile:",first;
		#print "median:",median;
		#print "third quartile:",third;
		#print "low:",array[1];
		#print "high:",array[NR];
		#print "std dev:",sqrt(sumsq/NR);
		#print "samples:",NR; 
		print count*2 "," array[1] "," first "," median "," third "," array[NR] >> filename;
	}' $fn
	count=$((count+1));
done
