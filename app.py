import customtkinter as ctk
import tkinter as tk
import tkinter.filedialog as fd
import subprocess
import threading
import os
import sys

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")


class TargetSelectionWindow(ctk.CTkToplevel):
    def __init__(self, parent, filename, valid_names):
        super().__init__(parent)
        self.title("Target Not Found")
        self.geometry("400x500")
        self.attributes("-topmost", True)
        self.valid_names = valid_names
        self.selected_target = None

        ctk.CTkLabel(
            self, 
            text=f"File: {filename}", 
            font=ctk.CTkFont(weight="bold"),
            wraplength=360,
            justify="center"
        ).pack(pady=(10, 0), padx=20)

        self.search_var = ctk.StringVar()
        self.search_var.trace("w", self.update_list)
        self.search_entry = ctk.CTkEntry(
            self, textvariable=self.search_var, placeholder_text="Search..."
        )
        self.search_entry.pack(fill="x", padx=10, pady=5)

        # Fast, standard Listbox for instant filtering
        self.listbox = tk.Listbox(
            self,
            bg="#2b2b2b",
            fg="#ffffff",
            selectbackground="#1f538d",
            borderwidth=0,
            highlightthickness=0,
            font=("Consolas", 11),
        )
        self.listbox.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self.listbox.bind("<Double-Button-1>", self.on_confirm)
        self.listbox.bind("<Return>", self.on_confirm)
        self.search_entry.bind("<Down>", lambda e: self.listbox.focus_set())

        self.update_list()

        btn_frame = ctk.CTkFrame(self, fg_color="transparent")
        btn_frame.pack(fill="x", padx=10, pady=10)
        ctk.CTkButton(btn_frame, text="Confirm", command=self.on_confirm).pack(
            side="right", padx=5
        )
        ctk.CTkButton(
            btn_frame,
            text="Skip File",
            command=self.on_skip,
            fg_color="#C62828",
            hover_color="#B71C1C",
        ).pack(side="right", padx=5)

        self.search_entry.focus()

    def update_list(self, *args):
        q = self.search_var.get().lower()
        self.listbox.delete(0, tk.END)
        for name in self.valid_names:
            if q in name.lower():
                self.listbox.insert(tk.END, name)
        if self.listbox.size() > 0:
            self.listbox.selection_set(0)

    def on_confirm(self, event=None):
        sel = self.listbox.curselection()
        if sel:
            self.selected_target = self.listbox.get(sel[0])
            self.destroy()

    def on_skip(self):
        self.selected_target = "SKIP"
        self.destroy()


class ZstdModderUI(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("ZstdModder")
        self.geometry("750x650")
        self.minsize(650, 550)

        # Smart Pathing: Works for both normal .py scripts AND PyInstaller .exe files
        if getattr(sys, "frozen", False):
            # If running as a compiled .exe, get the folder the .exe is sitting in
            script_dir = os.path.dirname(sys.executable)
        else:
            # If running as a standard .py script, get the folder the script is in
            script_dir = os.path.dirname(os.path.abspath(__file__))

        self.exe_name = (
            os.path.join(script_dir, "ZstdModder.exe")
            if sys.platform == "win32"
            else os.path.join(script_dir, "ZstdModder")
        )

        self.selected_files = []

        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(5, weight=1)

        self.title_label = ctk.CTkLabel(
            self, text="ZstdModder", font=ctk.CTkFont(size=24, weight="bold")
        )
        self.title_label.grid(
            row=0, column=0, columnspan=3, padx=20, pady=(20, 10), sticky="w"
        )

        self.atmo_label = ctk.CTkLabel(self, text="Atmosphere Path:")
        self.atmo_label.grid(row=1, column=0, padx=20, pady=5, sticky="e")
        self.atmo_entry = ctk.CTkEntry(
            self, placeholder_text=".../atmosphere/contents/[TitleID]"
        )
        self.atmo_entry.grid(row=1, column=1, padx=(0, 10), pady=5, sticky="ew")
        self.atmo_btn = ctk.CTkButton(
            self, text="Browse", command=self.browse_atmosphere, width=80
        )
        self.atmo_btn.grid(row=1, column=2, padx=(0, 20), pady=5)

        self.dump_label = ctk.CTkLabel(self, text="Game 'Stream' Folder:")
        self.dump_label.grid(row=2, column=0, padx=20, pady=5, sticky="e")
        self.dump_entry = ctk.CTkEntry(
            self, placeholder_text=".../romfs/Sound/Resource/Stream"
        )
        self.dump_entry.grid(row=2, column=1, padx=(0, 10), pady=5, sticky="ew")
        self.dump_btn = ctk.CTkButton(
            self, text="Browse", command=self.browse_dump, width=80
        )
        self.dump_btn.grid(row=2, column=2, padx=(0, 20), pady=5)

        self.audio_label = ctk.CTkLabel(self, text="Input Audio:")
        self.audio_label.grid(row=3, column=0, padx=20, pady=5, sticky="e")

        # Batch Input Controls
        self.file_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.file_frame.grid(row=3, column=1, columnspan=2, padx=0, pady=5, sticky="ew")
        self.file_status = ctk.CTkLabel(
            self.file_frame, text="0 file(s) selected", text_color="gray"
        )
        self.file_status.pack(side="left", padx=(0, 10))
        ctk.CTkButton(
            self.file_frame, text="Add Files", command=self.add_files, width=80
        ).pack(side="left", padx=5)
        ctk.CTkButton(
            self.file_frame, text="Add Folder", command=self.add_folder, width=80
        ).pack(side="left", padx=5)
        ctk.CTkButton(
            self.file_frame,
            text="Clear",
            command=self.clear_files,
            width=60,
            fg_color="gray",
        ).pack(side="left", padx=5)

        # Advanced Settings Frame (Cleaned up!)
        self.adv_frame = ctk.CTkFrame(self)
        self.adv_frame.grid(
            row=4, column=0, columnspan=3, padx=20, pady=10, sticky="ew"
        )
        self.overwrite_var = ctk.StringVar(value="on")
        ctk.CTkCheckBox(
            self.adv_frame,
            text="Overwrite Existing Mods",
            variable=self.overwrite_var,
            onvalue="on",
            offvalue="off",
        ).pack(side="left", padx=20, pady=10)

        self.console = ctk.CTkTextbox(
            self, font=ctk.CTkFont(family="Consolas", size=12)
        )
        self.console.grid(
            row=5, column=0, columnspan=3, padx=20, pady=10, sticky="nsew"
        )
        self.console.insert(
            "0.0",
            "[System] Ready. Select files to begin.\n(If no files are selected, the current folder will be scanned).\n",
        )
        self.console.configure(state="disabled")

        self.build_btn = ctk.CTkButton(
            self,
            text="Build Mod",
            font=ctk.CTkFont(size=14, weight="bold"),
            height=40,
            command=self.start_build,
        )
        self.build_btn.grid(
            row=6, column=0, columnspan=3, padx=20, pady=(10, 20), sticky="ew"
        )

        self.load_config()

    def add_files(self):
        files = fd.askopenfilenames(
            title="Select Audio Files",
            filetypes=[("Audio", "*.flac *.wav *.mp3 *.ogg")],
        )
        if files:
            self.selected_files.extend(files)
            self.file_status.configure(
                text=f"{len(self.selected_files)} file(s) selected", text_color="white"
            )

    def add_folder(self):
        folder = fd.askdirectory(title="Select Folder")
        if folder:
            for root, _, files in os.walk(folder):
                for f in files:
                    if f.lower().endswith((".flac", ".wav", ".mp3", ".ogg")):
                        self.selected_files.append(os.path.join(root, f))
            self.file_status.configure(
                text=f"{len(self.selected_files)} file(s) selected", text_color="white"
            )

    def clear_files(self):
        self.selected_files = []
        self.file_status.configure(text="0 file(s) selected", text_color="gray")

    def browse_atmosphere(self):
        d = fd.askdirectory(title="Select Atmosphere TitleID Folder")
        if d:
            self.atmo_entry.delete(0, "end")
            self.atmo_entry.insert(0, d)
            self.save_config()

    def browse_dump(self):
        d = fd.askdirectory(title="Select Game Stream Folder")
        if d:
            self.dump_entry.delete(0, "end")
            self.dump_entry.insert(0, d)
            self.save_config()

    def load_config(self):
        if os.path.exists("config.ini"):
            with open("config.ini", "r") as f:
                for line in f:
                    if line.startswith("ATMOSPHERE_PATH="):
                        self.atmo_entry.insert(0, line.strip().split("=")[1])
                    if line.startswith("GAME_DUMP_PATH="):
                        self.dump_entry.insert(0, line.strip().split("=")[1])

    def save_config(self):
        with open("config.ini", "w") as f:
            f.write(f"ATMOSPHERE_PATH={self.atmo_entry.get().strip()}\n")
            f.write(f"GAME_DUMP_PATH={self.dump_entry.get().strip()}\n")

    def log(self, message):
        self.console.configure(state="normal")
        self.console.insert("end", message)
        self.console.see("end")
        self.console.configure(state="disabled")

    def start_build(self):
        if not os.path.exists(self.exe_name):
            self.log(f"\n[Error] Cannot find {self.exe_name}.\n")
            return

        self.save_config()
        self.build_btn.configure(state="disabled", text="Building...")
        threading.Thread(target=self.run_process, daemon=True).start()

    def prompt_target_blocking(self, filename, valid_names):
        result = [None]
        event = threading.Event()

        def show_dialog():
            dialog = TargetSelectionWindow(self, filename, valid_names)
            self.wait_window(dialog)
            result[0] = dialog.selected_target
            event.set()

        self.after(0, show_dialog)
        event.wait()
        return result[0]


    def run_process(self):
        try:
            # 0. The SAFE way to hide windows without deadlocking C++ std::system calls!
            startupinfo = None
            if sys.platform == "win32":
                startupinfo = subprocess.STARTUPINFO()
                startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startupinfo.wShowWindow = 0  # 0 equals SW_HIDE (Invisible Window)

            # 1. Fetch valid BARS names silently from C++
            self.after(
                0,
                self.log,
                "\n--- Starting Batch Process ---\n[System] Parsing BARS targets...\n",
            )

            # CRITICAL FIX 1: stdin=subprocess.DEVNULL prevents the PyInstaller deadlock
            # CRITICAL FIX 2: startupinfo prevents the C++ std::system deadlock
            proc = subprocess.run(
                [self.exe_name, "--list"],
                capture_output=True,
                text=True,
                startupinfo=startupinfo,
                stdin=subprocess.DEVNULL,
            )

            # STRICT PARSING
            valid_names = []
            for line in proc.stdout.splitlines():
                if line.startswith("BARS_TARGET:"):
                    valid_names.append(line.replace("BARS_TARGET:", "").strip())

            if not valid_names:
                self.after(
                    0,
                    self.log,
                    "[Error] Failed to read BARS files. Are .bars.zs files in this directory?\n",
                )
                return

            # Determine files to process
            queue = self.selected_files
            if not queue:
                self.after(
                    0,
                    self.log,
                    "[System] No files explicitly selected. Scanning current directory...\n",
                )
                queue = [
                    f
                    for f in os.listdir(".")
                    if f.lower().endswith((".flac", ".wav", ".mp3", ".ogg"))
                ]

            if not queue:
                self.after(
                    0,
                    self.log,
                    "\n[Error] No audio files found! Please add files to begin.\n",
                )
                return

            processed_count = 0

            # 2. Process each file!
            for filepath in queue:
                basename = os.path.splitext(os.path.basename(filepath))[0]
                target_name = basename

                if target_name not in valid_names:
                    target_name = self.prompt_target_blocking(
                        os.path.basename(filepath), valid_names
                    )
                    if not target_name or target_name == "SKIP":
                        self.after(
                            0,
                            self.log,
                            f"[System] Skipped '{os.path.basename(filepath)}'\n",
                        )
                        continue

                cmd = [self.exe_name, filepath, "-t", target_name, "--no-patch"]
                if self.overwrite_var.get() == "on":
                    cmd.append("-y")

                p = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    stdin=subprocess.DEVNULL,
                    text=True,
                    errors="replace",
                    bufsize=1,
                    startupinfo=startupinfo,
                )
                for line in p.stdout:
                    self.after(0, self.log, line)
                p.wait()

                processed_count += 1

            if processed_count == 0:
                self.after(
                    0,
                    self.log,
                    "\n[System] No files were processed. Skipping BARS patcher.\n",
                )
                return

            # 3. Finalize Deployment
            self.after(0, self.log, "\n[System] Finalizing BARS Patching...\n")

            p = subprocess.Popen(
                [self.exe_name, "--patch-only"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL,
                text=True,
                errors="replace",
                bufsize=1,
                startupinfo=startupinfo,
            )
            for line in p.stdout:
                self.after(0, self.log, line)
            p.wait()

            self.after(0, self.log, "\n[System] Batch Job Complete!\n")

        except Exception as e:
            self.after(0, self.log, f"\n[Fatal Error] {str(e)}\n")
        finally:
            self.after(
                0, lambda: self.build_btn.configure(state="normal", text="Build Mod")
            )


if __name__ == "__main__":
    app = ZstdModderUI()
    app.mainloop()
