import matplotlib as plt
import numpy as np
import csv

def read_csv(input_csv):

	tags = []

	with open(input_csv) as csvfile:
		input_reader = csv.reader(csvfile, delimiter=',')
		i = 1	 

		while(type(input_reader[i][2]) == int):
			cpi = 1/(int(row([5])))
			#stall i corresponds to cyc_per i:
			tile_num = row[0]+ "_" + row[1]
			
			kern = {"tile": tile_num, "CPI": cpi, "stalls": [], "cpi_per": []}
			tags.append(kern)
			
			i+=1

		#skip miss stats
		while(input_reader[i][4] is not "% of Stall Cycles"):
			i+=1

		i+=1

		#add all stall cycles to appropriate dict
		while(input_reader[i][3] is not "% of Bubbles"):
                        tag_id = input_reader[i][2]
                        if tag_id is kernel:
 	                       tag_id = len(tags)

			tags[tag_id]["stalls"].append(input_reader[i][3])	
			stall_cpi = input_reader[i][5]*tags[tag_id]["CPI"]
			tags[tag_id]["cpi_per"].append(stall_cpi)
			
			i+=1
		i+=1		

		#add all bubble stats to appropriate dict
		while(input_reader[i][3] is not "Instruction"):
			tag_id = input_reader[i][2]
			if tag_id is kernel:
				tag_id = len(tags)

			tags[tag_id]["stalls"].append(input_reader[i][3])	
			bub_cpi = input_reader[i][5]*tags[tag_id]["CPI"]
			tags[tag_id]["cpi_per"].append(bub_cpi)

			i+=1
				
		return tags
     			
tags = read_csv("output.csv")

print(tags)



