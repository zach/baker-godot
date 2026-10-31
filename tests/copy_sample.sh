# Sourced by the tests: copy_sample DIR copies sample/ to DIR, without Godot's cache, and
# points the copy's C# project at the package's source where it is (BakerSample.csproj
# references it by a path relative to sample/).
copy_sample() {
    local to="$1" here_native
    cp -r "$here/sample/." "$to/"
    rm -rf "$to/.godot"
    here_native="$( (cygpath -m "$here" 2>/dev/null) || echo "$here")"
    sed -i.bak "s|\.\./csharp/|$here_native/csharp/|" "$to/BakerSample.csproj"
    rm -f "$to/BakerSample.csproj.bak"
}
