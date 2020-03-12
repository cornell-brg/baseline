import matplotlib.pyplot as plt
import numpy as np
import csv

def read_csv(input_csv):

	tags = {}

	with open(input_csv) as csvfile:
		reader = csv.reader(csvfile, delimiter=',')

		input_reader = []
		ct = 0
		for row in reader:
			input_reader.append(row)
			ct += 1

		i = 1	 

		while(i<ct):
			while(input_reader[i][4] != "miss"):
				cur_tag = input_reader[i][2]
				cpi = 1/(float(input_reader[i][5]))
				#stall i corresponds to cyc_per i:
				tile_num = input_reader[i][0]+ "_" + input_reader[i][1]
				
				tags[cur_tag] = {tile_num: {"CPI": cpi, "stalls": [], "cpi_per": []}}
				
				i+=1

			#skip miss stats
			while(input_reader[i][3] != "Stall Type"):
				i+=1

			i+=1

			#add all stall cycles to appropriate dict
			while(input_reader[i][3] != "Bubble Type"):
				tag_id = input_reader[i][2]
				tile_num = input_reader[i][0]+ "_" + input_reader[i][1]

				tags[tag_id][tile_num]["stalls"].append(input_reader[i][3])	
				stall_cpi = float(input_reader[i][5])*tags[tag_id][tile_num]["CPI"]*.01
				tags[tag_id][tile_num]["cpi_per"].append(stall_cpi)
				
				i+=1

			i+=1		

			#add all bubble stats to appropriate dict
			while(input_reader[i][3] != "Instruction"):
				tag_id = input_reader[i][2]
				tile_num = input_reader[i][0]+ "_" + input_reader[i][1]

				tags[tag_id][tile_num]["stalls"].append(input_reader[i][3])	
				bub_cpi = float(input_reader[i][5])*tags[tag_id][tile_num]["CPI"]*.01
				tags[tag_id][tile_num]["cpi_per"].append(bub_cpi)

				i+=1

			#skip lines until start of next tile stats
			while(i< ct and input_reader[i][5] != "IPC"):
				i+=1

			i+=1

				
		return tags
     			
tags = read_csv("output.csv")

for k, tag in enumerate(tags):
	fig, ax = plt.subplots()
	labels = []
	stall_tot = {}
	cpis = []
	width = 0.35       # the width of the bars: can also be len(x) sequence
	for c, t in enumerate(tags[tag]):
		cur_t = tags[tag][t]
		labels.append(t)
		stalls = cur_t["stalls"]
		cpis.extend(cur_t["cpi_per"])

	for i, s in enumerate(stalls):
		if c is 0:
			stall_tot[s] = [cpis[i]]
		else:
			stall_tot[s].append(cpis[i])


	prev_s = ""
	for i, s  in enumerate(stalls):
		if i is 0:
			ax.bar(labels, stall_tot[s],width, label=s)
			prev_s = s
		else:
			ax.bar(labels, stall_tot[s], width, bottom=stall_tot[prev_s],
		       label=s)
			prev_s = s

	ax.set_ylabel('CPI')
	ax.set_xlabel('Tile X,Y Coords')
	ax.set_title('CPI stack for tag ' + str(k+1))
	ax.legend()

	# plt.savefig("cpi_stack_tag_"+tag+".png")
	plt.show()




