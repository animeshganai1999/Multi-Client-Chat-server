import pickle
import sys
import numpy as np
import warnings 
# Settings the warnings to be ignored 
warnings.filterwarnings('ignore') 

# text = sys.argv[1] #reading command line argument: the argv array looks like this: ['testpy.py', 'abcd1234'] if cmd-line argument is abcd1234
text_lis = []
for i in range(1,len(sys.argv)):
    text_lis.append(sys.argv[i])
text = ' '.join(text_lis)

lis = [text]

classifier = pickle.load(open('saved_models/knnClassifier.pickle', "rb"))
vectorizer = pickle.load(open('saved_models/vectorizer.pickle', "rb"))

#dumping the contents into a temp file
with open('temp.txt','w') as f:
    X = vectorizer.transform(np.array(lis))
    print(X.shape)
    y = classifier.predict(X)
    if y == 1:
        f.write('1')
    else:
        f.write('0')
f.close()

