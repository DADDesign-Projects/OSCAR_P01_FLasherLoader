#!/usr/bin/env python3
"""
OSCAR Flasher Server — portage Python complet de cServer.cpp / OSCAR_Flasher_ServerDlg.cpp
Copyright (c) 2024-2026 Dad Design  (portage Python)

Dépendances :  pip install pyserial pillow elftools
"""

import os
import struct
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from PIL import Image as PILImage 
from elftools.elf.elffile import ELFFile
from elftools.elf.segments import Segment

import sys
from pathlib import Path

import serial
import serial.tools.list_ports

# ============================================================================
# Constantes (Flasher.h)
# ============================================================================
QSPI_SIZE        = 16_777_216          # 16 Mo
QSPI_PAGE_SIZE   = 4_096
QSPI_PAGE_COUNT  = 2_048
QSPI_ADRESSE     = 0x90000000

TRANS_BLOCK_SIZE = 1_024

MAX_ENTRY_NAME   = 40
DIR_FILE_COUNT   = 80

FILE_TYPE_BIN    = 0x2851
FILE_TYPE_IMG    = 0x2852
FILE_TYPE_ELF    = 0x2853

OFSF_EXT         = ".ofsf"

# Taille du répertoire en octets  (DIR_FILE_COUNT × (40 + 4 + 4 + 4))
_DIR_ENTRY_SIZE  = MAX_ENTRY_NAME + 4 + 4 + 4   # Name + Size + DataAddress + FileType
_DIR_SIZE        = DIR_FILE_COUNT * _DIR_ENTRY_SIZE

# Format struct d'une entrée répertoire (little-endian)
_DIR_ENTRY_FMT   = f"<{MAX_ENTRY_NAME}sIII"     # name, size, data_addr, file_type

# Format du bloc de transmission  ("BLOK" + NumBloc + CRC + EndTrans + Data[1024] + "END")
_BLOC_FMT        = f"<4sHBB{TRANS_BLOCK_SIZE}s3sx"
_BLOC_SIZE       = struct.calcsize(_BLOC_FMT)

# ----------------------------------------------------------------------------
# Format de fichier .ofsf (OSCAR Flasher Server File) — miroir de Flasher.h
#
#   [OFSFHeader]  [Directory: DirEntryCount x entrée]  [Data...]
#
# L'en-tête rend le format auto-descriptif : le nombre d'entrées du
# répertoire embarqué dans le fichier est lu depuis le fichier lui-même,
# et ne dépend plus de la valeur de DIR_FILE_COUNT au moment où on relit
# le fichier. Evite toute corruption silencieuse si DIR_FILE_COUNT change
# entre deux versions de l'outil (Python ou C++, le format est partagé).
# ----------------------------------------------------------------------------
OFSF_MAGIC        = b"OFSF"
OFSF_VERSION      = 1

_OFSF_HEADER_FMT  = "<4sII"      # magic, version, dir_entry_count
_OFSF_HEADER_SIZE = struct.calcsize(_OFSF_HEADER_FMT)


# ============================================================================
# cServer — portage de Dad::cServer
# ============================================================================
class cServer:
    """
    Gestion côté PC du protocole de flash QSPI.
    Reproduit fidèlement cServer.cpp de Dad Design.
    """

    def __init__(self):
        self._buf        = bytearray()      # image QSPI en RAM
        self._qspi_size  = 0
        self._index_file = 0               # nb de fichiers ajoutés
        self._port: serial.Serial | None = None

        # répertoire : liste de dicts {name, size, data_address, file_type}
        self._directory: list[dict] = []
        # données payload (après le répertoire)
        self._payload = bytearray()

        # Dernier message d'erreur détaillé (addFile OFSF / save_to_ofsf_file)
        self.last_error: str = ""

    # ── Initialisation ──────────────────────────────────────────────────────
    def Init(self, port_num: int, qspi_size: int = QSPI_PAGE_SIZE * 2,
             baudrate: int = 115200) -> bool:
        self.close()
        if qspi_size > QSPI_SIZE:
            return False
        self._qspi_size = qspi_size
        self._directory = []
        self._payload   = bytearray()
        self._index_file = 0

        if port_num == 0:
            # mode "save only" — pas de port série
            return True

        port_name = f"COM{port_num}" if os.name == "nt" else f"/dev/ttyS{port_num - 1}"
        try:
            self._port = serial.Serial(
                port_name, baudrate=baudrate,
                bytesize=8, parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0          # lecture non-bloquante (comme MAXDWORD côté C++)
            )
            return True
        except serial.SerialException:
            return False

    def close(self):
        if self._port and self._port.is_open:
            self._port.close()
        self._port = None

    # ── Protocole série ──────────────────────────────────────────────────────
    def Synchronize(self) -> int:
        """
        Attend la trame 'BLOK<nn>' du client OSCAR.
        Retourne le numéro de bloc attendu, ou -1 en cas d'erreur/timeout.
        Reproduit fidèlement la machine à états de cServer::Synchronize().
        """
        if not self._port:
            return -1

        MARKER = b"BLOK"
        step     = 0          # 0..3 = marqueur, 4 = octet bas, 5 = octet haut
        num_low  = 0
        deadline = time.monotonic() + 5.0   # 5 s

        while time.monotonic() < deadline:
            data = self._port.read(1)
            if not data:
                continue
            byte = data[0]

            if step < 4:
                if byte == MARKER[step]:
                    step += 1
                else:
                    step = 1 if byte == MARKER[0] else 0
            elif step == 4:
                num_low = byte
                step = 5
            else:  # step == 5
                num_bloc = num_low | (byte << 8)
                return num_bloc

        return -1   # timeout

    def TransBloc(self, num_bloc: int, end_trans: int = 0) -> bool:
        """
        Envoie un bloc de TRANS_BLOCK_SIZE octets au client OSCAR.
        Reproduit fidèlement cServer::TransBloc().
        """
        if not self._port:
            return False

        self._port.reset_input_buffer()

        full_buf = self._get_full_buffer()
        offset   = num_bloc * TRANS_BLOCK_SIZE
        data     = full_buf[offset: offset + TRANS_BLOCK_SIZE]
        # Compléter par des zéros si dernier bloc incomplet
        if len(data) < TRANS_BLOCK_SIZE:
            data = data + bytes(TRANS_BLOCK_SIZE - len(data))

        crc = sum(data) & 0xFF

        bloc_bytes = struct.pack(
            _BLOC_FMT,
            b"BLOK",           # StartMarker
            num_bloc,          # NumBloc  (uint16_t)
            crc,               # _CRC
            end_trans,         # _EndTrans
            bytes(data),       # Data[1024]
            b"END"             # EndMarker
        )

        try:
            written = self._port.write(bloc_bytes)
            self._port.flush() 
            return written == _BLOC_SIZE
        except serial.SerialException:
            return False

    # ── Chargement de fichiers ───────────────────────────────────────────────
    def addFile(self, file_path: str, file_name: str) -> bool:
        ext = os.path.splitext(file_name)[1].lower()
        if ext in (".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".gif"):
            return self._add_image_file(file_path, file_name)
        if ext == ".elf":
            return self._add_elf_file(file_path, file_name)
        if ext == ".ofsf":
            return self._add_ofsf_file(file_path, file_name)
        return self._add_common_file(file_path, file_name)

    # ── Requêtes sur le buffer ───────────────────────────────────────────────
    def getDataSize(self) -> int:
        return len(self._get_full_buffer())

    def getNbBlocs(self) -> int:
        return self.getDataSize() // TRANS_BLOCK_SIZE + 1

    def getBuffer(self) -> bytes:
        return bytes(self._get_full_buffer())

    # ── Buffer complet = répertoire sérialisé + payload ──────────────────────
    def _get_full_buffer(self) -> bytearray:
        dir_bytes = bytearray(_DIR_SIZE)
        for i, entry in enumerate(self._directory):
            name_bytes = entry["name"].encode("ascii", errors="replace")[:MAX_ENTRY_NAME - 1]
            name_bytes = name_bytes.ljust(MAX_ENTRY_NAME, b"\x00")
            struct.pack_into(_DIR_ENTRY_FMT, dir_bytes, i * _DIR_ENTRY_SIZE,
                             name_bytes,
                             entry["size"],
                             entry["data_address"],
                             entry["file_type"])
        return dir_bytes + self._payload

    # ── Adresse QSPI d'un offset payload ─────────────────────────────────────
    def _qspi_addr(self) -> int:
        """Adresse QSPI du prochain octet libre dans le payload."""
        return QSPI_ADRESSE + _DIR_SIZE + len(self._payload)

    # ── Alignement 4 octets ───────────────────────────────────────────────────
    @staticmethod
    def _align4(buf: bytearray):
        pad = (4 - len(buf) % 4) % 4
        buf += b"\x00" * pad

    # ── uint32_t little-endian ────────────────────────────────────────────────
    @staticmethod
    def _u32le(v: int) -> bytes:
        return struct.pack("<I", v)

    # ── Fichier binaire générique ─────────────────────────────────────────────
    def _add_common_file(self, file_path: str, file_name: str) -> bool:
        try:
            with open(file_path, "rb") as f:
                data = f.read()
        except OSError:
            return False

        if self._index_file >= DIR_FILE_COUNT:
            return False
        if len(self._payload) + len(data) + _DIR_SIZE > self._qspi_size:
            return False

        addr = self._qspi_addr()
        self._payload += data
        self._align4(self._payload)

        self._directory.append({
            "name":         file_name,
            "size":         len(data),
            "data_address": addr,
            "file_type":    FILE_TYPE_BIN,
        })
        self._index_file += 1
        return True

    # ── Fichier image (PNG, JPEG, GIF animé…) ────────────────────────────────
    def _add_image_file(self, file_path: str, file_name: str) -> bool:
        try:
            img = PILImage.open(file_path)
        except Exception:
            return False

        # Vérifier si GIF animé
        frames = []
        try:
            while True:
                frames.append(img.copy().convert("RGBA"))
                img.seek(img.tell() + 1)
        except EOFError:
            pass

        if not frames:
            frames = [img.convert("RGBA")]

        width, height = frames[0].size
        nb_frames     = len(frames)
        pixel_bytes   = width * height * 4 * nb_frames
        magic_bytes   = 16
        total_size    = pixel_bytes + magic_bytes

        if self._index_file >= DIR_FILE_COUNT:
            return False

        addr = self._qspi_addr()

        for frame in frames:
            raw = frame.tobytes("raw", "BGRA")   # GDI+ stocke BGRA
            self._payload += raw

        # Bloc magique "IMAG"
        self._payload += b"IMAG"
        self._payload += self._u32le(nb_frames)
        self._payload += self._u32le(width)
        self._payload += self._u32le(height)

        self._directory.append({
            "name":         file_name,
            "size":         total_size,
            "data_address": addr,
            "file_type":    FILE_TYPE_IMG,
        })
        self._index_file += 1
        return True

    # ── Fichier ELF ARM ───────────────────────────────────────────────────────
    def _add_elf_file(self, file_path: str, file_name: str) -> bool:
        try:
            with open(file_path, "rb") as f:
                raw_elf = f.read()
        except OSError:
            return False

        # Parser les segments PT_LOAD
        try:
            from io import BytesIO
            elf = ELFFile(BytesIO(raw_elf))
            if elf.get_machine_arch() != "ARM":
                return False

            regions = []
            for seg in elf.iter_segments():
                if seg.header.p_type == "PT_LOAD":
                    regions.append({
                        "file_offset":  seg.header.p_offset,
                        "dest_addr":    seg.header.p_vaddr,
                        "source_addr":  seg.header.p_paddr,
                        "copy_size":    seg.header.p_filesz,
                        "zero_size":    seg.header.p_memsz - seg.header.p_filesz,
                    })
        except Exception:
            return False

        if self._index_file >= DIR_FILE_COUNT:
            return False

        addr       = self._qspi_addr()
        file_size  = len(raw_elf)
        magic_size = 8 + len(regions) * 20

        if len(self._payload) + file_size + magic_size + _DIR_SIZE > self._qspi_size:
            return False

        self._payload += raw_elf

        # Bloc magique "ELF0"
        self._payload += b"ELF0"
        self._payload += self._u32le(len(regions))
        for r in regions:
            self._payload += self._u32le(r["file_offset"])
            self._payload += self._u32le(r["dest_addr"])
            self._payload += self._u32le(r["source_addr"])
            self._payload += self._u32le(r["copy_size"])
            self._payload += self._u32le(r["zero_size"])

        self._align4(self._payload)

        self._directory.append({
            "name":         file_name,
            "size":         file_size,    # taille ELF brut seulement (comme C++)
            "data_address": addr,
            "file_type":    FILE_TYPE_ELF,
        })
        self._index_file += 1
        return True

    # ── Fichier OFSF (ré-importation d'une image sauvegardée) ────────────────
    def _add_ofsf_file(self, file_path: str, file_name: str) -> bool:
        self.last_error = ""
        try:
            with open(file_path, "rb") as f:
                raw = f.read()
        except OSError as e:
            self.last_error = f"OFSF: unable to open '{file_name}' ({e})"
            return False

        # -- Lecture et validation de l'en-tête ------------------------------
        if len(raw) < _OFSF_HEADER_SIZE:
            self.last_error = (
                f"OFSF: '{file_name}' is too small to contain a valid header "
                "(old format or corrupted file - please regenerate it)"
            )
            return False

        magic, version, dir_entry_count = struct.unpack_from(_OFSF_HEADER_FMT, raw, 0)

        if magic != OFSF_MAGIC:
            self.last_error = (
                f"OFSF: '{file_name}' has no valid OFSF header "
                "(old format generated before header versioning - please regenerate it)"
            )
            return False

        if version != OFSF_VERSION:
            self.last_error = (
                f"OFSF: '{file_name}' has unsupported format version "
                f"{version} (expected {OFSF_VERSION})"
            )
            return False

        # Garde-fou : évite une lecture absurde sur un fichier corrompu
        if dir_entry_count == 0 or dir_entry_count > DIR_FILE_COUNT:
            self.last_error = (
                f"OFSF: '{file_name}' has an invalid directory entry count "
                f"({dir_entry_count})"
            )
            return False

        dir_bytes = dir_entry_count * _DIR_ENTRY_SIZE
        if _OFSF_HEADER_SIZE + dir_bytes > len(raw):
            self.last_error = f"OFSF: '{file_name}' is truncated (directory larger than file)"
            return False

        embedded_dir_raw = raw[_OFSF_HEADER_SIZE: _OFSF_HEADER_SIZE + dir_bytes]
        payload_data      = raw[_OFSF_HEADER_SIZE + dir_bytes:]

        # Décalage à appliquer aux adresses du répertoire source pour les
        # rebaser à leur nouvelle position dans notre buffer hôte. On utilise
        # dir_bytes (taille RÉELLE du répertoire source, lue depuis le fichier)
        # et non la constante DIR_FILE_COUNT courante.
        current_offset = _DIR_SIZE + len(self._payload)

        # -- Copie des entrées non-nulles du répertoire embarqué -------------
        for i in range(dir_entry_count):
            entry_offset = i * _DIR_ENTRY_SIZE
            name_b, size, data_addr, file_type = struct.unpack_from(
                _DIR_ENTRY_FMT, embedded_dir_raw, entry_offset
            )
            if size == 0:
                break
            if self._index_file >= DIR_FILE_COUNT:
                self.last_error = f"OFSF: host directory full while merging '{file_name}'"
                return False

            name = name_b.rstrip(b"\x00").decode("ascii", errors="replace")
            relative = data_addr - (QSPI_ADRESSE + dir_bytes)
            new_addr = QSPI_ADRESSE + current_offset + relative

            self._directory.append({
                "name":         name,
                "size":         size,
                "data_address": new_addr,
                "file_type":    file_type,
            })
            self._index_file += 1

        # -- Copie des données --------------------------------------------------
        if len(self._payload) + len(payload_data) + _DIR_SIZE > self._qspi_size:
            self.last_error = f"OFSF: not enough space in QSPI buffer to load '{file_name}'"
            return False

        self._payload += payload_data
        self._align4(self._payload)
        return True

    # ── Sauvegarde .ofsf versionnée ───────────────────────────────────────────
    def save_to_ofsf_file(self, file_path: str) -> bool:
        """
        Écrit le buffer courant (Répertoire + Data) dans un fichier .ofsf,
        préfixé d'un en-tête versionné (OFSFHeader), afin qu'il puisse
        toujours être relu en toute sécurité via addFile()/_add_ofsf_file(),
        quelle que soit la valeur future de DIR_FILE_COUNT.
        """
        self.last_error = ""
        header = struct.pack(_OFSF_HEADER_FMT, OFSF_MAGIC, OFSF_VERSION, DIR_FILE_COUNT)
        try:
            with open(file_path, "wb") as f:
                f.write(header)
                f.write(self._get_full_buffer())
            return True
        except OSError as e:
            self.last_error = f"OFSF: write error while saving '{file_path}' ({e})"
            return False

def resource_path(relative_path: str) -> str:
    """
    Return absolute path to resource.
    Compatible with normal execution and PyInstaller.
    """
    if getattr(sys, "frozen", False):
        base_path = Path(sys._MEIPASS)
    else:
        base_path = Path(__file__).parent

    return str(base_path / relative_path)

# ============================================================================
# Interface graphique — fidèle à la capture d'écran
# ============================================================================
class OSCARFlasherServerDlg(tk.Tk):

    def __init__(self):
        super().__init__()
        self.iconbitmap(resource_path("OSCAR_Flasher_Server.ico"))
        self.title("OSCAR PY Flasher Server")
        self.resizable(False, False)
        self._flash_thread: threading.Thread | None = None
        self._port_data: list[str] = []
        self._build_ui()
        self._refresh_com_ports()
        self._set_status(
            "Choose :\n"
            " - a communication port,\n"
            " - the files to be\n" 
            "   transferred to OSCAR's \n"
            "   QSPI Flash memory.\n\n"
            "Click Flash"
        )

    # ── Construction de l'UI ────────────────────────────────────────────────
    def _build_ui(self):
        PAD = 8
        tk.Label(self, text="Files List").place(x=PAD, y=PAD)

        frame_files = tk.Frame(self, bd=1, relief=tk.SUNKEN)
        frame_files.place(x=PAD, y=28, width=400, height=240)
        sy = tk.Scrollbar(frame_files, orient=tk.VERTICAL)
        sx = tk.Scrollbar(frame_files, orient=tk.HORIZONTAL)
        self.lst_files = tk.Listbox(frame_files,
                                     yscrollcommand=sy.set,
                                     xscrollcommand=sx.set,
                                     selectmode=tk.SINGLE)
        sy.config(command=self.lst_files.yview)
        sx.config(command=self.lst_files.xview)
        sy.pack(side=tk.RIGHT,  fill=tk.Y)
        sx.pack(side=tk.BOTTOM, fill=tk.X)
        self.lst_files.pack(fill=tk.BOTH, expand=True)

        tk.Button(self, text="Add File",    width=10, command=self._on_add_file   ).place(x=PAD,       y=278)
        tk.Button(self, text="Delete File", width=10, command=self._on_delete_file).place(x=PAD + 100, y=278)
        tk.Button(self, text="Save Files",  width=10, command=self._on_save_files ).place(x=PAD + 210, y=278)

        tk.Label(self, text="COM Port").place(x=418, y=PAD)

        frame_com = tk.Frame(self, bd=1, relief=tk.SUNKEN)
        frame_com.place(x=418, y=28, width=90, height=240)
        sc = tk.Scrollbar(frame_com, orient=tk.VERTICAL)
        self.lst_com = tk.Listbox(frame_com, yscrollcommand=sc.set, exportselection=False)
        sc.config(command=self.lst_com.yview)
        sc.pack(side=tk.RIGHT, fill=tk.Y)
        self.lst_com.pack(fill=tk.BOTH, expand=True)
        self.lst_com.bind("<<ListboxSelect>>", self._on_com_selected)

        tk.Button(self, text="Refresh", width=10, command=self._refresh_com_ports).place(x=418, y=278)

        self.btn_flash = tk.Button(self, text="Flash", width=31, height=3, command=self._on_flash)
        self.btn_flash.place(x=526, y=28)

        self.progress = ttk.Progressbar(self, orient=tk.HORIZONTAL, length=226, mode="determinate")
        self.progress.place(x=526, y=96)

        self.txt_status = tk.Text(self, width=28, height=8, state=tk.DISABLED,
                                   bd=1, relief=tk.SUNKEN, wrap=tk.WORD)
        self.txt_status.place(x=524, y=130)

        tk.Label(self, text="OSCAR Flasher Server Version 1.3\n"
                            "Copyright (C) DADDesign-Project 2024-26\n"
                            "https://github.com/DADDesign-Projects",
                 ).place(x=520, y=264)
        
        self.geometry("760x320")

    # ── Helpers ─────────────────────────────────────────────────────────────
    def _set_status(self, message: str):
        self.txt_status.config(state=tk.NORMAL)
        self.txt_status.delete("1.0", tk.END)
        self.txt_status.insert(tk.END, message)
        self.txt_status.config(state=tk.DISABLED)

    def _enable_flash(self):
        self.after(0, lambda: self.btn_flash.config(state=tk.NORMAL))

    def _update_progress(self, value: int, pct: int):
        self.progress["value"] = value
        self._set_status(f"Transfer in progress \n{pct}% completed")

    # ── Boutons ──────────────────────────────────────────────────────────────
    def _on_add_file(self):
        if self.lst_files.size() >= DIR_FILE_COUNT:
            messagebox.showwarning("Error Add File", "Maximum file number size reached")
            return
        paths = filedialog.askopenfilenames(title="Select files")
        for path in paths:
            if self.lst_files.size() >= DIR_FILE_COUNT:
                messagebox.showwarning("Error Add File", "Maximum file number size reached")
                break
            self.lst_files.insert(tk.END, path)

    def _on_delete_file(self):
        sel = self.lst_files.curselection()
        if sel:
            idx  = sel[0]
            name = self.lst_files.get(idx)
            if messagebox.askyesno("Delete file in the list", f"Delete : {name} ?"):
                self.lst_files.delete(idx)

    def _refresh_com_ports(self):
        self.lst_com.delete(0, tk.END)
        self._port_data = []
        for port in sorted(serial.tools.list_ports.comports(), key=lambda p: p.device):
            self.lst_com.insert(tk.END, port.device)
            self._port_data.append(port.device)

    def _on_com_selected(self, _event=None):
        sel = self.lst_com.curselection()
        if not sel:
            return
        port_name = self._port_data[sel[0]]
        port_num  = self._port_num(port_name)

        server = cServer()
        if server.Init(port_num):
            result = server.Synchronize()
            if result == -1:
                self._set_status(f"OSCAR not detected on {port_name}")
            else:
                self._set_status(f"OK : OSCAR is detected\non {port_name}")
            server.close()
        else:
            messagebox.showerror("COM Error", f"{port_name} is not accessible")

    def _on_flash(self):
        if self._flash_thread and self._flash_thread.is_alive():
            return
        self.btn_flash.config(state=tk.DISABLED)
        self._flash_thread = threading.Thread(target=self._flash_worker, daemon=True)
        self._flash_thread.start()

    def _on_save_files(self):
        nb = self.lst_files.size()
        if nb == 0:
            messagebox.showwarning("No file selected", "Please add a file")
            return

        save_path = filedialog.asksaveasfilename(
            title="Save all files in the list to an OFSF file",
            defaultextension=OFSF_EXT,
            filetypes=[("OSCAR Flasher Server Files", f"*{OFSF_EXT}"), ("All Files", "*.*")],
        )
        if not save_path:
            return
        if not save_path.lower().endswith(OFSF_EXT):
            save_path += OFSF_EXT

        server = cServer()
        server.Init(0, QSPI_SIZE)

        for i in range(nb):
            path = self.lst_files.get(i)
            name = os.path.basename(path)
            if not server.addFile(path, name):
                detail = server.last_error or f"Error loading {name} file"
                messagebox.showerror("File loading error", detail)
                self._set_status("File loading error")
                return

        if not server.save_to_ofsf_file(save_path):
            messagebox.showerror("Error", server.last_error or "Write error")
            return

        buf = server.getBuffer()
        messagebox.showinfo("Success", f"File saved successfully.\n{len(buf)} bytes written.")

    # ── Thread Flash ─────────────────────────────────────────────────────────
    def _flash_worker(self):
        nb_files = self.lst_files.size()
        if nb_files == 0:
            messagebox.showwarning("No file selected", "Please add a file")
            self._enable_flash()
            return

        sel = self.lst_com.curselection()
        if not sel:
            messagebox.showwarning("No COM port selected", "Please select a COM port")
            self._enable_flash()
            return

        port_name = self._port_data[sel[0]]
        port_num  = self._port_num(port_name)

        server = cServer()
        if not server.Init(port_num, QSPI_SIZE):
            messagebox.showerror("COM Error", f"{port_name} is not accessible")
            self._enable_flash()
            return

        # Chargement des fichiers
        for i in range(nb_files):
            path = self.lst_files.get(i)
            name = os.path.basename(path)
            if not server.addFile(path, name):
                detail = server.last_error or f"Error loading {name} file"
                messagebox.showerror("File loading error", detail)
                self._set_status("File loading error")
                self._enable_flash()
                server.close()
                return

        # Transfert
        nb_blocs = server.getNbBlocs()
        self.after(0, lambda: self.progress.config(maximum=nb_blocs, value=0))

        for bloc in range(nb_blocs):
            synchro = server.Synchronize()
            if synchro == -1:
                messagebox.showerror(
                    "Transfer error",
                    "Synchronization with OSCAR impossible (Maybe Change COM Port?)")
                self._set_status("Unable to communicate with OSCAR")
                self._enable_flash()
                server.close()
                return

            if bloc != synchro:
                messagebox.showerror(
                    "Transfer error",
                    "Communication problem detected, flashing in progress stopped.")
                self._set_status("File transfer or flash\nprocedure failed")
                self._enable_flash()
                server.close()
                return

            end_trans = 1 if bloc == nb_blocs - 1 else 0
            if not server.TransBloc(bloc, end_trans):
                messagebox.showerror(
                    "Transfer error",
                    "Communication problem detected (COM port inaccessible), flashing in progress stopped.")
                self._set_status("File transfer or flash\nprocedure failed")
                self._enable_flash()
                server.close()
                return

            v   = bloc + 1
            pct = v * 100 // nb_blocs
            self.after(0, lambda v=v, p=pct: self._update_progress(v, p))

        self._set_status("Transfer and flash completed")
        self._enable_flash()
        server.close()

    # ── Utilitaire ───────────────────────────────────────────────────────────
    @staticmethod
    def _port_num(port_name: str) -> int:
        digits = "".join(filter(str.isdigit, port_name))
        return int(digits) if digits else 0


# ============================================================================
# Point d'entrée
# ============================================================================
if __name__ == "__main__":
    app = OSCARFlasherServerDlg()
    app.mainloop()
