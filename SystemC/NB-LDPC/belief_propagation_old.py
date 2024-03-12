import galois as gl
import numpy as np
import math

"""
This file contains an implemntation of a generalized belief propagation algorithm for NB-LDPC decoding. 
The assumption is made that the all-zero codeword is transmitted.    
"""

def build_H(N:int,K:int,weight:int, q:int) -> gl.FieldArray:
    """
    Builds a random NB-LDPC parity check matrix.
    
    Args:
        N (int): transmitted block length
        K (int): source block length
        weight (int): mean weight of each column, must be greater than 2
        q (int): order of the Galois field
    """
    assert N > K
    assert weight > 2
    M = N - K # number of parity check equations
    GF = gl.GF(q)
    H = GF(np.zeros((M,N),dtype=int)) # inirialize parity check matrix
    for i in range(N):
        for _ in range(weight):
            idx = np.random.randint(0,M) # select a random row
            while H[idx,i] != GF(0):
                idx = np.random.randint(0,M)
            H[idx,i] = GF(np.random.randint(1,q-1)) # select a random non-zero element
    return H 

def simulate_channel(N: int, sigma: float, mu:int, q: int) -> gl.FieldArray:
    """
    Generates a random noise sample vector.
    
    Args:
        N (int): transmitted block length
        sigma (float): standard deviation of the Gaussian distribution
        mu (int): mean of the Gaussian distribution
        q (int): order of the Galois field
        
    Returns:
        gl.FieldArray: noise sample vector
    """
    GF = gl.GF(q)
    noise = np.sign(np.random.normal(0,sigma,int(N*math.log2(q)))) # Generate noise bits and make decision
    print(noise)
    noise = [int(x) for x in np.where(noise == -1, 0, noise)] # Convert to binary
    samples = GF(np.zeros(N,dtype=int))
    idx = 0
    for i in range(0,int(N*math.log2(q)),int(math.log2(q))):
        sample = noise[i:i+int(math.log2(q))]
        sample = int(''.join([str(x) for x in sample]),2)
        samples[idx] = GF(sample)
        idx += 1
    return samples

def likelihood(y: float, sigma: float, bit: int) -> float:
    """_summary_
    
    Calculates the likelihood of a bit being 1 or 0 given the received sample. Assumes s = 1

    Args:
        y (float): received sample
        sigma (float): standard deviation of the Gaussian distribution
        bit (int): bit to calculate the likelihood for

    Returns:
        float: likelihood of the bit being 1 or 0
    """
    assert bit in range(2)
    g = 1/(1+math.exp(2*abs(y)/sigma**2 ))
    return g if bit == 1 else 1-g    

def belief_propagation(H: gl.FieldArray, samples: gl.FieldArray, max_iter: int, p: int, sigma: float) -> gl.FieldArray:
    """
    Performs belief propagation decoding on a NB-LDPC code assuming the all-zero codeword is transmitted.
    
    Args:
        H (galois.FieldArray): parity check matrix
        samples (galois.FieldArray): received samples
        max_iter (int): maximum number of iterations
        q (int): order of the Galois field
        sigma (float): standard deviation of the Gaussian distribution
    """
    M,N = H.shape # get the dimensions of the parity check matrix, M is the number of parity check equations, N is the block length
    GF = gl.GF(p) # create a Galois field of order p
    
    # Initialize messages
    q = [(np.zeros((M,N),dtype=int)) for a in range(p)] # q is the probability that symbol n is a, one for each element in the Galois field
    r = [(np.zeros((M,N),dtype=int)) for a in range(p)] # r is the probability that check m is satisfied if sample n is set to a, one for each element in the Galois field
    
    # Initialize q
    s = 1
    for m in range(M):
        for n in range(N):
            if H[m,n] != GF(0):
                for element in range(p):
                    ele_bin = bin(int(element))[2:].zfill(int(math.log2(p)))
                    print(ele_bin)
                    f = 1
                    for bit in ele_bin:
                        f *= likelihood(samples[n],sigma,int(bit))
                    q[element][m,n] = f
                        
    # Decode
    iter = 0
    while(iter < max_iter):
        # Update r
        for m in range(M):
            for n in range(N):
                if H[m,n] != GF(0):
                    for element in range(p):
                        stored_sample = samples[n]
                        samples[n] = GF(element)
                        if GF(1) in samples or GF(2) in samples or GF(3) in samples:
                            # prob of zm given samples is 0
                            r[element][m,n] = 0
                            samples[n] = stored_sample
                            continue
                        else:
                            # calculate product of adjacent q's
                            prod = 1
                            for i in range(N):
                                if i == n or H[m,i] == GF(0):
                                    continue
                                prod *= q[element][m,i]
                            r[element][m,n] = prod
                            samples[n] = stored_sample
        
        # update q
        for m in range(M): # For each check node
            for n in range(N): # And for each symbol node
                if H[m,n] != GF(0): # If the check node is connected to the symbol node
                    alpha = 0
                    for element in range(p): # Then for each element in the Galois field
                        alpha += q[element][m,n]
                    alpha = 1/alpha # Calculate normalization factor
                    for element in range(p): # Then for each element in the Galois field
                        prod = 1
                        for j in range(M): # For each check node
                            if j == m or H[j,n] == GF(0):
                                continue
                            prod *= r[element][j,n] # Calculate product of adjacent r's
                        likelihood = likelihood(samples[n],sigma,element)
                        q[element][m,n] = alpha * prod * likelihood # Update q
        
        # Tentative decision
        elements = [x for x in range(p)]
        for n in range(N): # for each sample
            check_probs = []
            for element in range(p):
                check_probs[element] = likelihood(samples[n],sigma,element)
                for m in range(M):
                    if H[m,n] != GF(0):
                        check_probs[element] *= r[element][m,n] # Calculate product of adjacent r's
            samples[n] = GF(elements.index(np.argmax(check_probs))) # Make decision
        print(samples)
        # Check if the tentative decision is a valid codeword
        if np.all(np.dot(H,samples) == GF(0)):
            return samples
        else: # If not, continue decoding
            iter += 1              
        
    decision = GF(np.zeros(N,dtype=int))         
    return decision
    
H = build_H(10,5,3,4)

    
        
