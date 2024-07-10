import numpy as np
from scipy.signal import butter, cheby1, cheby2, ellip, sosfilt, sosfreqz
import matplotlib.pyplot as plt

# Sample rate and desired cutoff frequencies (in Hz)
fs = 30.0
lowcut = 0.5
highcut = 1.5   # was 2.0

# Normalize the frequencies
nyq = 0.5 * fs
low = lowcut / nyq
high = highcut / nyq

# Design Butterworth bandpass filter
order = 3  # Filter order
sos = butter(order, [low, high], btype='band' , output='sos')
rp = 10.5   # Passband ripple in dB
rs = 60    # Stopband attenuation in dB
#sos = ellip(order, rp, rs, [low, high], btype='band', output='sos')
#sos = cheby1(order, rp, [low, high], btype='band', output='sos')
#sos = cheby1(order, rs, [low, high], btype='band', output='sos')

# Function to generate C++ sos definition
def generate_cpp_sos(sos):
    cpp_sos = "std::vector<std::vector<double>> sos = {\n"
    for section in sos:
        cpp_sos += "    {" + ", ".join(f"{coef:.8f}" for coef in section) + "},\n"
    cpp_sos = cpp_sos.rstrip(",\n") + "\n};"
    return cpp_sos
print (generate_cpp_sos(sos))

# Frequency response
w, h = sosfreqz(sos, worN=2000)
plt.plot((fs * 0.5 / np.pi) * w, abs(h), 'b')
plt.plot([0, 0.5, 0.5, 2, 2, fs / 2], [0, 0, 1, 1, 0, 0], 'r--')
plt.xlim(0, 5)
plt.title("Bandpass Filter Frequency Response")
plt.xlabel('Frequency [Hz]')
plt.ylabel('Gain')
plt.grid()
plt.show()

# Example of filtering a signal
t = np.linspace(0, 10, 10 * fs, endpoint=False)
a = 0.02
f0 = 1.0
x = 0.1 * np.sin(2 * np.pi * 0.3 * t) + 0.3 * np.sin(2 * np.pi * f0 * t) + a * np.random.normal(size=t.shape)

# Apply filter
y = sosfilt(sos, x)

# Plot the original and filtered signals
plt.figure(figsize=(10, 6))
plt.plot(t, x, label='Noisy signal')
plt.plot(t, y, label='Filtered signal')
plt.xlabel('Time [seconds]')
plt.grid()
plt.legend()
plt.show()

