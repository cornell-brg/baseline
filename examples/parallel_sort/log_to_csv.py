import sys, os
import re
import csv

filepaths = ['kernel/v2/stats/tile/tile_0_0_stats.log',
'kernel/v2/stats/tile/tile_1_2_stats.log',
'kernel/v2/stats/tile/tile_2_3_stats.log',
]

'''
    Processes the given chunk of information using the
    given file pointer @f
    Notice that @f is incremented in this function
'''
def process_chunk(f, chunk_name, coordY, coordX):
    print('processing chunk ', chunk_name)
    result = []

    # get column names
    line = f.readline().strip()
    titles = re.split('\s\s+', line)

    # skip ===== separator
    line = f.readline().strip()
    if re.match('\s*=+\s*', line):
        line = f.readline().strip()

    # grab data: change these lists to modify which columns to grab
    data_titles = ['coordY', 'coordX', 'tagid'] # unique identifier for this tile/tag
    if chunk_name == 'Per-Tile Stats':
        data_titles += []
    elif chunk_name == 'Per-Tile Timing Stats':
        data_titles += ['instr', 'cycle', 'IPC']
    elif chunk_name == 'Per-Tile Miss Stats':
        data_titles += titles
    elif chunk_name == 'Per-Tile Stall Stats':
        data_titles += titles
    elif chunk_name == 'Per-Tile Bubble Stats':
        data_titles += titles
    elif chunk_name == 'Instruction Stats':
        data_titles += titles

    print('data_titles',data_titles)
    result.append(data_titles)
    tagid = 'kernel'
    while not re.match('\s*=+\s*', line): # look for next =====
        # skip ---- Tag x ---- separator
        m = re.match('-+\s*Tag\s+(?P<tagid>[a-zA-Z0-9]+)\s*-+', line)
        if m:
            tagid = m.group('tagid')
            line = f.readline().strip()
            continue

        # get coordY, coordX, tagid
        contents = re.split('\s\s+', line)
        data = []
        if chunk_name == 'Per-Tile Timing Stats':
            coordM = re.match('(\d)\s*,\s*(\d)', contents[0])
            data.append(coordM.group(1)) # coordY
            data.append(coordM.group(2)) # coordX
        else:
            data.append(coordY)
            data.append(coordX)
        data.append(tagid) # tagid

        # get other requested data
        for idx,t in enumerate(data_titles[3:]): # skip coordY, coordX, tagid
            data.append(contents[titles.index(data_titles[idx+3])])

        print('contents', contents)
        print('data', data)
        result.append(data)
        line = f.readline().strip()

    return result

'''
    Reads the given @file_path for data
    Skips invalid files
    Returns a list of lists of the data in this file
'''
def read_file(file_path):
    if not os.path.exists(file_path):
        print(file_path + ' is not a valid directory... skipping')
        return []

    result = []

    with open(file_path, 'r') as f:
        line = f.readline().strip()
        coordY = '0'
        coordX = '0'
        while line:
            if re.match('^Per-Tile\sStats$', line):
                process_chunk(f, 'Per-Tile Stats', coordY, coordX)
                #result += process_chunk(f, 'Per-Tile Stats', coordY, coordX)
            elif re.match('^Per-Tile\sTiming\sStats$', line):
                data = process_chunk(f, 'Per-Tile Timing Stats', coordY, coordX)
                coordY = data[1][0]
                coordX = data[1][1]
                result += data
            elif re.match('^Per-Tile\sMiss\sStats$', line):
                result += process_chunk(f, 'Per-Tile Miss Stats', coordY, coordX)
            elif re.match('^Per-Tile\sStall\sStats$', line):
                result += process_chunk(f, 'Per-Tile Stall Stats', coordY, coordX)
            elif re.match('^Per-Tile\sBubble\sStats$', line):
                result += process_chunk(f, 'Per-Tile Bubble Stats', coordY, coordX)
            elif re.match('^Instruction\sStats$', line):
                result += process_chunk(f, 'Instruction Stats', coordY, coordX)

            line = f.readline()

    #print(result)
    return result

if __name__ == '__main__':
    if len(sys.argv) > 1:
        filepaths = sys.argv[1:]

    result = []
    for fp in filepaths:
        result += read_file(fp)

    with open('output.csv', 'w+', newline='') as f:
        writer = csv.writer(f)
        writer.writerows(result)
