<h1 align="center">
  <br>
  <img src=assets/nanika.jpg alt=":3" width="300"></a>
  <br>
  <br>
</h1>

@tlsbollei

A research-grade PoC toolkit demonstrating five CET-compliant syscall evasion techniques that bypass modern, commercial and world class EDRs on Windows 11 (SentinelOne Singularity XDR, CrowdStrike Falcon).

## Includes what?
Includes gadget discovery, Spectre-style hook probes, and a first-public-sight in-kernel eBPF JIT abuse, all open-source and reproducible. Academic research paper in a .pdf format, along with .tex source code, included.

## Note from author
This project is released strictly for educational, academic, and red team research purposes. The techniques shown are meant to help improve system defenses — not to aid or promote unauthorized access, malware development, or exploitation. Use responsibly, ethically, and only in lab environments or with explicit permission. You are solely responsible for any use of this code.
That is why, as a result, the eBPF JIT driver abuse PoC comes pre-shipped with a minimal high-level skeleton of both the kernel-land driver and user-mode loader, as a full proof of concept implmenentation would wreak carnage,
