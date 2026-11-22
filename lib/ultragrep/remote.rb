require 'open3'
require 'fileutils'
require 'ultragrep/executor'
require 'tmpdir'

module Ultragrep
  class Remote < Executor
    def initialize(options, config)
      super(options, config)
      @hosts = config.remote_hosts
    end

    def bin_path
      "~/.ultragrep_remote"
    end

    def lua
      "~/.ultragrep_remote/#{File.basename(@config.lua)}"
    end

    def popen(host, cat_cmd, filter)
      cmd = ["ssh", host, "#{cat_cmd} | #{filter}"]

      if debug?
        debug_cmd = cmd.dup
        debug_cmd[2] = '"' + debug_cmd[2] + '"'
        $stderr.puts(debug_cmd.join(' '))
      end

      IO.popen(cmd)
    end

    def get_children_pipes(regexps)
      files = collect_files
      pipes = []
      files.each do |host, files|
        if @options[:tail]
          remote_cat = ["echo @@FILE:#{files.join}", ";", 'tail', '-f'] + files
        else
          remote_cat = [ug_cat, @options[:range_start], "~/.ultragrep_remote/indexes", lua] + files
        end
        remote_guts = [ug_guts, "-l", lua, "-s", @options[:range_start], "-e", @options[:range_end], regexps]

        pipes << [popen(host, remote_cat.join(' '), remote_guts.join(' ')), host + ":"]
      end
      pipes
    end

    def setup!
      @hosts.each do |host|
        $stderr.puts("setting up state on '#{host}'")
        system_dbg("ssh", host, "mkdir -p ~/.ultragrep_remote/indexes") || raise("Couldn't make remote dir on #{host}!")

        binaries = static_binaries(remote_arch(host))
        system_dbg("scp", *binaries, host + ":.ultragrep_remote/") || raise("Couldn't scp binaries to #{host}!")

        system_dbg("scp", @config.local_lua_path, host + ":.ultragrep_remote/") || raise("Coudln't scp #{@config.local_lua_path} to #{host}")
      end
    end

    DOCKER_PLATFORMS = {
      "x86_64"  => "linux/amd64",
      "amd64"   => "linux/amd64",
      "aarch64" => "linux/arm64",
      "arm64"   => "linux/arm64",
    }

    BINARIES = %w(ug_guts ug_cat)

    def remote_arch(host)
      arch = syscall("ssh", host, "uname -m") || raise("Couldn't determine architecture of #{host}!")
      DOCKER_PLATFORMS[arch] || raise("Unsupported architecture '#{arch}' on #{host}")
    end

    # Builds static linux binaries in a docker image and copies them out of a
    # throwaway container into build/<platform>/.  Done once per platform per run.
    def static_binaries(platform)
      @static_binaries ||= {}
      @static_binaries[platform] ||= build_binaries(platform)
    end

    def build_binaries(platform)
      root = File.join(File.dirname(__FILE__), "..", "..")

      out_dir = File.join(Dir.mktmpdir, platform.tr("/", "-"))
      FileUtils.mkdir_p(out_dir)

      image = "ultragrep-static:#{platform.tr("/", "-")}"

      $stderr.puts("building static binaries for #{platform}")
      system_dbg("docker", "build", "--quiet", "--platform", platform, "-t", image,
                  "-f", File.join(root, "docker", "linux-static.Dockerfile"), root, :out => File::NULL) ||
        raise("Couldn't build docker image for #{platform} (is docker running?)")

      container = syscall("docker", "create", "--platform", platform, image, "true") ||
        raise("Couldn't create container from #{image}")

      BINARIES.map do |bin|
        dest = File.join(out_dir, bin)
        system_dbg("docker", "cp", "#{container}:/out/#{bin}", dest) || raise("Couldn't copy #{bin} out of #{image}")
        dest
      end
    ensure
      system_dbg("docker", "rm", container, :out => File::NULL) if container
    end

    def collect_files
      files = {}
      glob = @config.log_path_glob

      @hosts.each do |host|
        host_files = []
        if @options[:tail]
          host_files << @config.primary_log
        else
          glob.each do |g|
            ssh_files = syscall("ssh", host, "ls -1tc #{g} | tac").split("\n")
            ssh_files.each do |f|
              host_files << f
            end
          end
        end
        files[host] ||= []
        files[host] += host_files
      end
      files
    end

    def system_dbg(*args)
      $stderr.puts(args.join(' ')) if debug?
      system(*args)
    end

    def syscall(*cmd)
      $stderr.puts(cmd.join(' ')) if debug?
      stdout, stderr, status = Open3.capture3(*cmd)
      status.success? && stdout.slice!(0..-(1 + $/.size)) # strip trailing eol
    end

  end
end
