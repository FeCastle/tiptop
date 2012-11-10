from sys import argv

"""
Usage test_tiptop_basic tiptop.output
"""

def main():   
    i = 0
    tiptop_output = open (argv[1])   
    
    for line in tiptop_output:
        if i > 5:
            print line.strip('\n')
            info = line.split()
            print info
        i = i + 1
        
    tiptop_output.close()
    
if __name__ == "__main__":
    main()

