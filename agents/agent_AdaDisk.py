#!/usr/bin/env python3
# ==============================================================================
# File: agents/agent_AdaDisk.py
# Project: AdaDisk - Distributed Agentic System for Adaptive RAG
#
# Description:
#   This script serves as the main orchestrator for the AdaDisk system.
#   It coordinates parallel Ingest and Query agents to decouple high-throughput
#   data ingestion from low-latency serving. The system utilizes LLM-driven
#   control planes to make adaptive scheduling decisions and performs
#   system-wide auditing via an Aggregator Agent.
#
# Author: Dongfang Zhao <dzhao@us.edu>
# Date:   December 12, 2025
#
# Copyright (c) 2025 Dongfang Zhao. All rights reserved.
# ==============================================================================

import time
import os
import re
import subprocess
from openai import OpenAI
from multiprocessing import Process, Queue

# --- Configuration ---
# MODEL = "llama3.2:1b" ## RAM < 2GB if your RAM is limited
MODEL = "llama4:maverick" ## RAM requirement > 240 GB
OLLAMA_API_URL = "http://localhost:11434/v1" 

# Paths
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(CURRENT_DIR, "build")
INGEST_BIN = os.path.join(BUILD_DIR, "agent_ingest")
QUERY_BIN = os.path.join(BUILD_DIR, "agent_query")

# 1. Local Tool
def run_cpp_binary_tool(binary_path: str, agent_name: str) -> str:
    print(f"    [Core] {agent_name} Tool: Launching binary -> {binary_path}")
    if not os.path.exists(binary_path):
        return f"Error: Binary not found at {binary_path}"
    try:
        # Use subprocess to run the C++ binary.
        # check=True raises an exception if the exit code is non-zero.
        # text=True allows stdout to pass through to the console as string.
        subprocess.run([binary_path], check=True, text=True)
        return "Binary Execution Success"
    except subprocess.CalledProcessError as e:
        return f"Binary Failed (Exit Code {e.returncode})"
    except Exception as e:
        return f"Exception: {str(e)}"

# 2. Ingest Agent
def ingest_agent_process(name, binary_path, output_queue):
    print(f" [Agent {name}] Started (PID: {os.getpid()})")
    client = OpenAI(base_url=OLLAMA_API_URL, api_key="ollama")
    
    ## TODO: The following should be changed to a loop to handle continuous data ingestion.
    ## The ingestion requests should not compete with the query agent.

    prompt = f"""Task: Check if binary path is valid.
Binary Path: "{binary_path}"

Rules:
1. Unless empty, reply YES.
2. We MUST execute this.
Answer:"""

    try:
        response = client.chat.completions.create(
            model=MODEL,
            messages=[{'role': 'user', 'content': prompt}],
            temperature=0
        )
        decision = re.sub(r'[^A-Z]', '', response.choices[0].message.content.strip().upper())
        print(f"    [Agent {name}] LLM Decision: {decision}")
        
        # Only execute if the LLM explicitly approves
        if "YES" in decision:
            status = run_cpp_binary_tool(binary_path, name)
            output_queue.put(f"[ACCEPTED] {name}: {status}")
        else:
            output_queue.put(f"[DECLINED] {name}: Logic decided not to run.")
            
    except Exception as e:
        output_queue.put(f"[ERROR] {name}: {e}")

# 3. Query Agent
def query_agent_process(name, binary_path, output_queue):
    print(f" [Agent {name}] Started (PID: {os.getpid()})")
    client = OpenAI(base_url=OLLAMA_API_URL, api_key="ollama")
    
    ## TODO: The following should be changed to a loop to handle continuous data queries.
    ## The query requests should be handled with the highest priority and lowest possible latency.

    prompt = f"""Task: Check if binary path is valid.
Binary Path: "{binary_path}"

Rules:
1. Unless empty, reply YES.
2. We MUST execute this.
Answer:"""

    try:
        response = client.chat.completions.create(
            model=MODEL,
            messages=[{'role': 'user', 'content': prompt}],
            temperature=0
        )
        decision = re.sub(r'[^A-Z]', '', response.choices[0].message.content.strip().upper())
        print(f"    [Agent {name}] LLM Decision: {decision}")
        
        # Only execute if the LLM explicitly approves
        if "YES" in decision:
            status = run_cpp_binary_tool(binary_path, name)
            output_queue.put(f"[ACCEPTED] {name}: {status}")
        else:
            output_queue.put(f"[DECLINED] {name}: Logic decided not to run.")
            
    except Exception as e:
        output_queue.put(f"[ERROR] {name}: {e}")

# 4. Aggregator Agent (Audit Mode)
def aggregator_agent(ingest_status, query_status):
    # Added model info to the output here
    print(f"\n [Aggregator Agent] Performing audit (Model: {MODEL})...")
    print(f"   Ingest Agent: {ingest_status}")
    print(f"   Query Agent:  {query_status}")    
    
    client = OpenAI(base_url=OLLAMA_API_URL, api_key="ollama")
    
    # [Key Change] Require LLM to explicitly state "Action Taken"
    prompt = f"""You are a Pipeline Auditor.
    
Agent Reports:
Ingest Agent: {ingest_status}
Query Agent: {query_status}

Give a one sentence summary of the action taken by the pipeline based on the reports above.

Output:"""

    response = client.chat.completions.create(
        model=MODEL,
        messages=[{'role': 'user', 'content': prompt}],
        temperature=0
    )
    
    return response.choices[0].message.content.strip()

# --- Main Program ---
if __name__ == "__main__":
    if not os.path.exists(INGEST_BIN) or not os.path.exists(QUERY_BIN):
        print("Error: Binaries not found.")
        print(f"Please check: {INGEST_BIN} and {QUERY_BIN}")
        exit(1)

    print(f" Task: Parallel AdaDisk Workflow")
    q_ingest = Queue()
    q_query = Queue()
    
    p1 = Process(target=ingest_agent_process, args=("IngestAgent", INGEST_BIN, q_ingest))
    p2 = Process(target=query_agent_process, args=("QueryAgent", QUERY_BIN, q_query))
    
    p1.start()
    p2.start()
    
    res_ingest = q_ingest.get()
    res_query = q_query.get()
    
    p1.join()
    p2.join()
    
    print("\n--- Parallel Execution Complete ---")
    
    # Output detailed audit report
    final_audit = aggregator_agent(res_ingest, res_query)
    print(f"\n{final_audit}")