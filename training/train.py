"""
Trains a tiny llama2.c model on data/officer.txt and exports it for the ESP32.

Steps: train a 512-token sentencepiece vocab, tokenize every exchange with a BOS
token in front (the firmware stops generating when the model emits the next BOS),
train the model on CPU, then export model.bin / tok.bin in llama2.c's legacy format.

Usage: python train.py [--iters 8000] [--out ../data]
"""

import argparse
import math
import os
import sys
import time

import numpy as np
import sentencepiece as spm
import torch

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "llama2c"))
from model import ModelArgs, Transformer  # noqa: E402
from export import legacy_export  # noqa: E402
from tokenizer import Tokenizer  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SEP = "\n<|sep|>\n"


def train_vocab(docs, prefix, vocab_size):
    # same settings llama2.c uses for its custom TinyStories vocabs; byte_fallback
    # matters because the firmware encodes unknown characters as byte tokens (id = byte + 3)
    text_file = prefix + "_vocab.txt"
    with open(text_file, "w") as f:
        f.write("\n".join(docs))
    spm.SentencePieceTrainer.train(
        input=text_file, model_prefix=prefix, model_type="bpe", vocab_size=vocab_size,
        self_test_sample_size=0, input_format="text", character_coverage=1.0,
        num_threads=os.cpu_count(), split_digits=True, allow_whitespace_only_pieces=True,
        byte_fallback=True, unk_surface=r" \342\201\207 ", normalization_rule_name="identity",
    )
    os.remove(text_file)
    return prefix + ".model"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=os.path.join(HERE, "data/officer.txt"))
    parser.add_argument("--out", default=os.path.join(HERE, "..", "data"))
    parser.add_argument("--name", default="officer")
    parser.add_argument("--iters", type=int, default=8000)
    parser.add_argument("--batch", type=int, default=64)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--seq-len", type=int, default=128)  # matches CONFIG_LLM_MAX_SEQ_LEN
    args = parser.parse_args()
    torch.manual_seed(1337)
    torch.set_num_threads(os.cpu_count())

    docs = [d.strip() for d in open(args.data).read().split(SEP) if d.strip()]
    print(f"{len(docs)} exchanges")

    work = os.path.join(HERE, "work")
    os.makedirs(work, exist_ok=True)
    sp_model = train_vocab(docs, os.path.join(work, args.name), 512)
    sp = spm.SentencePieceProcessor(model_file=sp_model)

    ids = []
    for d in docs:
        ids.append(sp.bos_id())
        ids.extend(sp.encode(d))
    ids.append(sp.bos_id())
    data = torch.tensor(np.array(ids, dtype=np.int64))
    n_val = len(data) // 20
    train_data, val_data = data[:-n_val], data[-n_val:]
    print(f"{len(data)} tokens, {len(data) / len(docs):.1f} per exchange")

    # same shape as stories260K: dim 64, 5 layers, 8 heads, 4 kv heads, hidden 172
    margs = ModelArgs(dim=64, n_layers=5, n_heads=8, n_kv_heads=4, vocab_size=512,
                      multiple_of=4, max_seq_len=args.seq_len, dropout=0.0)
    model = Transformer(margs)
    print(f"{sum(p.numel() for p in model.parameters()) / 1e3:.0f}K parameters")
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=0.1, betas=(0.9, 0.95))

    def batch(split):
        src = train_data if split == "train" else val_data
        ix = torch.randint(len(src) - args.seq_len - 1, (args.batch,))
        x = torch.stack([src[i:i + args.seq_len] for i in ix])
        y = torch.stack([src[i + 1:i + 1 + args.seq_len] for i in ix])
        return x, y

    @torch.no_grad()
    def val_loss():
        model.eval()
        losses = []
        for _ in range(20):
            x, y = batch("val")
            model(x, y)
            losses.append(model.last_loss.item())
        model.train()
        return sum(losses) / len(losses)

    warmup = 200
    start = time.time()
    for it in range(args.iters + 1):
        # linear warmup then cosine decay to 10%
        lr = args.lr * min(1.0, (it + 1) / warmup)
        if it > warmup:
            lr = args.lr * (0.1 + 0.9 * 0.5 * (1 + math.cos(math.pi * (it - warmup) / (args.iters - warmup))))
        for g in opt.param_groups:
            g["lr"] = lr
        x, y = batch("train")
        model(x, y)
        opt.zero_grad(set_to_none=True)
        model.last_loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        if it % 250 == 0:
            elapsed = time.time() - start
            eta = elapsed / (it + 1) * (args.iters - it)
            print(f"iter {it:5d} | train {model.last_loss.item():.3f} | val {val_loss():.3f} | "
                  f"lr {lr:.1e} | {elapsed / 60:.1f} min, ~{eta / 60:.1f} min left", flush=True)

    os.makedirs(args.out, exist_ok=True)
    torch.save({"model": model.state_dict(), "args": margs}, os.path.join(work, args.name + ".pt"))
    legacy_export(model, os.path.join(args.out, args.name + ".bin"))
    Tokenizer(sp_model).export()  # writes work/<name>.bin next to the .model
    os.replace(os.path.join(work, args.name + ".bin"), os.path.join(args.out, args.name + "_tok.bin"))
    print(f"exported {args.name}.bin and {args.name}_tok.bin to {os.path.abspath(args.out)}")


if __name__ == "__main__":
    main()
